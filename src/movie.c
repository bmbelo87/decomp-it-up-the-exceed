/*
 * movie.c — player dos .MOV (formato MOV2) do Exceed.
 *
 * Original (exceed.exe):
 *   0x4225A8  abre: "MOV2", pula hdr[0x88], chave 16 B, nome cifrado 32 B, byte G
 *   0x4226A0  lê 0x1000 B e desembaralha: b = bitrev(b) ^ bitrev(G) (0x4223C4/0x422420)
 *   0x422A68  decodifica até o quadro-alvo (tempo x fps) com a libmpeg2:
 *             estado 1 (SEQUENCE) -> mpeg2_convert(rgb16); 0 (BUFFER) -> mais dados;
 *             7/8 (SLICE/END) -> quadro pronto. No fim do arquivo volta a hdr[0x88]+0xC0.
 *   Quadro final RGB16 640x480.
 *
 * O port usa a própria MPEG2.dll do jogo (mesma API), carregada com LoadLibrary.
 */
#include "pumpy.h"
#include "movie.h"
#include "mpeg2.h"
#include "mpeg2convert.h"

#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif

static struct {
    FILE*               f;
    mpeg2dec_t*         dec;
    const mpeg2_info_t* info;
    uint32_t            dataStart;     /* hdr[0x88] + 0xC0 */
    uint8_t             table[256];    /* bitrev(b) ^ bitrev(G) */
    uint8_t             buf[0x1000];
    double              fps;
    double              time;
    int                 target;        /* [+0x2c] no original */
    int                 decoded;       /* [+0x30] */
    bool                loop;
    bool                ended;
    GLuint              tex;
    bool                hasFrame;
} g_mov;

static uint8_t bitrev8(uint8_t b) {                                /* 0x4223C4 */
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) if (b & (1 << i)) r |= (uint8_t)(0x80 >> i);
    return r;
}

/* 0x4226D8: fps pelo frame_rate_code do sequence header */
static double movie_fps(const uint8_t* s) {
    static const double rates[8] = { 24000.0/1001, 24, 25, 30000.0/1001, 30, 50, 60000.0/1001, 60 };
    if (s[0] == 0 && s[1] == 0 && s[2] == 1 && s[3] == 0xB3) {
        int code = s[7] & 0x0F;
        if (code >= 1 && code <= 8) return rates[code - 1];
    }
    return 30000.0 / 1001;
}

static int movie_read(void) {                                      /* 0x4226A0 */
    int n = (int)fread(g_mov.buf, 1, sizeof(g_mov.buf), g_mov.f);
    for (int i = 0; i < n; i++) g_mov.buf[i] = g_mov.table[g_mov.buf[i]];
    return n;
}

bool Movie_Open(const char* path, bool loop) {
    Movie_Close();

    char cleanPath[MAX_PATH];
    strncpy(cleanPath, path, sizeof(cleanPath) - 1);
    cleanPath[sizeof(cleanPath) - 1] = '\0';
#if !defined(_WIN32)
    for (char* p = cleanPath; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#endif

    FILE* f = fopen(cleanPath, "rb");
    if (!f) { Log_Print("MOVIE: falha ao abrir '%s'\n", cleanPath); return false; }
    uint8_t hdr[0x8C];
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) || memcmp(hdr, "MOV2", 4) != 0) {
        Log_Print("MOVIE: '%s' nao e MOV2\n", cleanPath);
        fclose(f); return false;
    }
    uint32_t n = *(uint32_t*)(hdr + 0x88);
    uint8_t tail[0x34];
    fseek(f, (long)n, SEEK_CUR);
    if (fread(tail, 1, sizeof(tail), f) != sizeof(tail)) { fclose(f); return false; }
    uint8_t k = bitrev8(tail[0x30]);
    for (int b = 0; b < 256; b++) g_mov.table[b] = (uint8_t)(bitrev8((uint8_t)b) ^ k);

    g_mov.f = f;
    g_mov.dataStart = n + 0xC0;
    fseek(f, (long)g_mov.dataStart, SEEK_SET);
    uint8_t first[8];
    size_t got = fread(first, 1, sizeof(first), f);
    for (size_t i = 0; i < got; i++) first[i] = g_mov.table[first[i]];
    g_mov.fps = movie_fps(first);
    fseek(f, (long)g_mov.dataStart, SEEK_SET);

    g_mov.dec = mpeg2_init();
    if (!g_mov.dec) { fclose(f); g_mov.f = NULL; return false; }
    g_mov.info = mpeg2_info(g_mov.dec);
    g_mov.loop = loop;
    g_mov.time = 0;
    g_mov.target = 0;
    g_mov.decoded = 0;
    g_mov.ended = false;
    g_mov.hasFrame = false;
    if (!g_mov.tex) glGenTextures(1, &g_mov.tex);
    Log_Print("MOVIE: '%s' aberto (N=0x%X, %.3f fps, loop=%d)\n", cleanPath, n, g_mov.fps, loop);
    return true;
}

void Movie_Close(void) {
    if (g_mov.dec) { mpeg2_close(g_mov.dec); g_mov.dec = NULL; }
    if (g_mov.f) { fclose(g_mov.f); g_mov.f = NULL; }
    g_mov.hasFrame = false;
}

bool Movie_IsOpen(void) { return g_mov.f != NULL; }
bool Movie_HasEnded(void) { return g_mov.ended; }
int  Movie_GetDecoded(void) { return g_mov.decoded; }

static void movie_upload(void) {
    if (!g_mov.info) return;
    const mpeg2_fbuf_t* fb = g_mov.info->display_fbuf;
    const mpeg2_sequence_t* sq = g_mov.info->sequence;
    if (!fb || !fb->buf[0] || !sq) return;
    glBindTexture(GL_TEXTURE_2D, g_mov.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    if (!g_mov.hasFrame) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)sq->width, (GLsizei)sq->height, 0,
                 GL_RGB, GL_UNSIGNED_SHORT_5_6_5, fb->buf[0]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    g_mov.hasFrame = true;
}

/* 0x422A68: decodifica até decoded > target */
void Movie_Update(float dt) {
    if (!g_mov.f || g_mov.ended) return;
    g_mov.time += dt;
    g_mov.target = (int)(g_mov.time * g_mov.fps);
    bool newFrame = false;
    while (g_mov.decoded <= g_mov.target) {
        mpeg2_state_t st = mpeg2_parse(g_mov.dec);
        if (st == STATE_SEQUENCE) {
            mpeg2_convert(g_mov.dec, mpeg2convert_rgb16, NULL);
        } else if (st == STATE_BUFFER) {
            int n = movie_read();
            if (n <= 0) {
                if (!g_mov.loop) {
                    Log_Print("MOVIE: fim do arquivo (%d quadros)\n", g_mov.decoded);
                    g_mov.ended = true; break;
                }
                fseek(g_mov.f, (long)g_mov.dataStart, SEEK_SET);
                n = movie_read();
                if (n <= 0) { g_mov.ended = true; break; }
            }
            mpeg2_buffer(g_mov.dec, g_mov.buf, g_mov.buf + n);
        } else if (st == STATE_SLICE || st == STATE_END) {
            g_mov.decoded++;
            newFrame = true;
        }
    }
    if (newFrame) movie_upload();
}

/* Tela cheia 640x480. Projeção Y-UP: linha 0 do quadro (topo) vai em y=480. */
void Movie_Render(void) {
    if (!g_mov.hasFrame) return;
    /* Restaura o blend no fim: o gameplay desenha por cima contando com o
     * estado que estava ligado (sem isso os sprites saem com fundo). */
    GLboolean blend = glIsEnabled(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, g_mov.tex);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 480);
    glTexCoord2f(1, 0); glVertex2f(640, 480);
    glTexCoord2f(1, 1); glVertex2f(640, 0);
    glTexCoord2f(0, 1); glVertex2f(0, 0);
    glEnd();
    if (blend) glEnable(GL_BLEND);
}
