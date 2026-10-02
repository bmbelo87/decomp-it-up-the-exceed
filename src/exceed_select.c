/*
 * exceed_select.c — tela de seleção de música do Exceed (CSelect).
 *
 * Carga (0x4161E6..0x41624F):
 *   BGA\SELECT.DAT  -> [this+4]  (bgaPics[0])
 *   BGA\SELECT2.DAT -> [this+8]  (bgaPics[1])  — overlay de modificadores/dificuldade
 *   BGA\90.DAT      -> "%X.TGA" de cada uma das 105 músicas, handle em 0x456B44
 *
 * Campos do objeto (usados aqui):
 *   +0x2C frame global         +0x34 frame do canal ([this+0x30]+4)
 *   +0x3C frame do cursor      +0x5C canal  +0x60 canal anterior  +0x64 direção do canal
 *   +0x68[ch] cursor por canal +0x74[ch] cursor anterior           +0x80 direção do cursor
 *
 * Desenho (0x416760), nesta ordem:
 *   SetProjection(43.603) — 2*atan(240/600): o plano z=0 fica 1:1 com 640x480
 *   slots 0,1,2 (ro_bar, Ro_L, Ro_R) no quadro f % 240
 *   slots 14 (top) e 18 (time) no quadro 30
 *   slot 15+canal (T_glow) no quadro (t % 180) + 60
 *   rótulo do canal: slot 0x456ED4[ch] = 19/20/21, base 0x456EC8[ch] = 60/120/180
 *   carrossel (0x4195D8) + slot 3/4/5 (screen_s / screen)
 *   rotação do canal (Xrot*) ou fundo parado (slot 24 = main_s, quadro 30)
 *   banner central (0x41983C)
 *   SetOrtho
 *   slots 25,26,27 (Mglow, glow1, glow1) no quadro t % 240, slot 49 (panel) no 30
 *   slot 43 (logo) e 44 (light): f, e depois de 100 = (f-100) % 120 + 100
 *
 * Ainda não reproduzido: contador de tempo (0x41EDB0), ícones de modificador
 * (SELECT2), painel de dificuldade, sons, confirmação da música.
 */
#include "pumpy.h"
#include "bga.h"

#define SEL_BGA   0   /* [this+4] */
#define SEL2_BGA  1   /* [this+8] */

static int  g_bannerTex[EX_SONG_COUNT];     /* 0x456B40[i].tex, -1 = sem banner */
static int  g_list[EX_CHANNEL_MAX];         /* 0x563A00 */
static int  g_listCount;                    /* 0x563AD0 */

static int  g_frame;        /* +0x2C */
static int  g_chFrame;      /* +0x34 */
static int  g_curFrame;     /* +0x3C */
static int  g_ch;           /* +0x5C */
static int  g_chPrev;       /* +0x60 */
static int  g_chDir;        /* +0x64: 0 parado, 1 canal-1, 2 canal+1 */
static int  g_cursor[EX_CHANNEL_COUNT];     /* +0x68 */
static int  g_cursorPrev[EX_CHANNEL_COUNT]; /* +0x74 */
static int  g_curDir;       /* +0x80: 0 parado, 1 cursor-1, 2 cursor+1 */

/* Painel de dificuldade (1 jogador) */
static bool g_chosen;       /* +0x95: música escolhida, painel aberto */
static bool g_armed;        /* +0x94: CENTER já apertado uma vez no painel */
static int  g_panelFrame;   /* +0x44: +1 por quadro com o painel aberto */
static int  g_panelIdx;     /* +0x88: índice entre os modos disponíveis */
static int  g_panelDir;     /* +0x58: 0 parado, 1 índice-1, 2 índice+1 */
static int  g_modeCount;    /* +0x98 */
static int  g_modeList[6];  /* 0x56390C (1P) / 0x563920 (2P): modo por índice */
static bool g_panel2P;      /* painel dos dois jogadores (0x418A55 / 0x418D35) */
static int  g_battleDiff;   /* +0x8C: dificuldade da linha BATTLE (0 N, 1 H, 2 C) */
static int  g_fontTex = -1; /* [obj+0x48C00]: font.tga do BGA\00.DAT (0x404555) */
static int  g_timerTex = -1;    /* [0x568F8C]: font4.tga do BGA\BFONT.DAT (0x41E60B) */
static DWORD g_startTick;       /* cronômetro [this+0x18] (0x426940 em 0x4162A7) */
static int  g_timeLeft;         /* +0xA0: segundos restantes */
static bool g_started;          /* comando RUN já emitido */
static int  g_repeat;       /* +0xA8: movimentos seguidos do cursor com DL/DR segurado */
static bool g_previewOn;    /* +0x84: preview AUDIO\D%X.AUD já disparado */
static bool g_intro;        /* estado 1 da Select (0x4165A4) antes do principal */

static int wrapCursor(int c);


/* ── songDB sintético do Exceed ─────────────────────────────────────────────
 * Nomes de modo = seções do STX que step.c já conhece (Step_SelectChart):
 * NORMAL 1, HARD 2, CRAZY 4, DOUBLE 5 (FREESTYLE, "-d"), NIGHTMARE 3 ("-nm").
 * Mesma ordem de ExceedSong.level. */
bool g_exceedSongIds = false;
static const char* k_dbModeName[5] = { "NORMAL", "HARD", "CRAZY", "DOUBLE", "NIGHTMARE" };

const char* Song_IdStr(int id) {
    static char buf[4][16];
    static int slot;
    char* s = buf[slot++ & 3];
    snprintf(s, 16, g_exceedSongIds ? "%X" : "%d", id);
    return s;
}

/* Ocultas desbloqueadas por código: A26 (Oh! Rosa!) e A27 (First Love) usam o
 * TITLE, o BGA e o STEP da 401 e da 402; só o .AUD (e a prévia D*.AUD) é o delas.
 * Regra deste projeto, pedida pelo usuário: o exceed.exe não tem esse desvio. */
int Song_DataId(int id) {
    if (!g_exceedSongIds) return id;
    if (id == 0xA26) return 0x401;
    if (id == 0xA27) return 0x402;
    return id;
}

const char* Song_DataIdStr(int id) {
    return Song_IdStr(Song_DataId(id));
}

void ExSelect_BuildSongDB(SongDB* db) {
    memset(db, 0, sizeof(*db));
    g_exceedSongIds = true;
    for (int i = 0; i < EX_SONG_COUNT && i < MAX_SONGS; i++) {
        SongEntry* e = &db->songs[db->songCount++];
        e->id = (int)g_exSongs[i].id;
        e->bpm = (float)g_exSongs[i].bpm;
        const char* t = g_exSongs[i].titleEn[0] ? g_exSongs[i].titleEn : g_exSongs[i].titleKr;
        strncpy(e->title, t, sizeof(e->title) - 1);
    }
    for (int m = 0; m < 5; m++) {
        SongMode* md = &db->modes[db->modeCount++];
        strncpy(md->name, k_dbModeName[m], sizeof(md->name) - 1);
        for (int i = 0; i < EX_SONG_COUNT && md->songCount < MAX_SONGS_PER_MODE; i++) {
            if (g_exSongs[i].level[m] == -1) continue;
            md->songIds[md->songCount] = (int)g_exSongs[i].id;
            md->difficulties[md->songCount] = g_exSongs[i].level[m];
            md->songCount++;
        }
    }
    Log_Print("EXSELECT: songDB sintético: %d músicas, %d modos\n", db->songCount, db->modeCount);
}

/* ── Códigos de comando (0x455140..0x455192, verificador 0x4155AC / 0x415958) ── */
#define EXMOD_X2        0x0002
#define EXMOD_X3        0x0004
#define EXMOD_X4        0x0008
#define EXMOD_X8        0x0010
#define EXMOD_V         0x0020
#define EXMOD_M         0x0040
#define EXMOD_R         0x0080
#define EXMOD_NS        0x0100
#define EXMOD_200       0x0200   /* só é limpo pelos códigos; sem ícone */
#define EXMOD_RV        0x0400
#define EXMOD_800       0x0800   /* sem ícone, efeito não identificado */
#define EXMOD_1000      0x1000   /* sem ícone, limpa velocidade; efeito não identificado */
#define EXMOD_UNLOCK    0x2000   /* libera as ocultas em 0x4192F0 */
#define EX_XMODE        0x8000   /* [0x568FF4]: X-MODE, global */

static unsigned g_joined;       /* [0x568FF4] bits 0/1 */
static unsigned g_flags;        /* [0x568FF4] acima de 0xFFF (X-MODE) */
bool ExSelect_IsXMode(void) { return (g_flags & 0x8000u) != 0; }
static unsigned g_mods[2];      /* +0x184 de cada jogador */
static int      g_joinFrame[2]; /* +0x4C (P1) / +0x54 (P2) */
static uint8_t  g_buf9[2][9];   /* 0x5638E0 */
static uint8_t  g_buf5[2][5];   /* 0x5638F4 */
static uint8_t  g_buf6[2][6];   /* 0x563900 */

static const uint8_t k_code6[6] = { 1, 2, 1, 2, 1, 2 };                  /* 0x455140 */
static const uint8_t k_code5[2][5] = {                                   /* 0x455148 */
    { 8, 16, 8, 16, 4 },
    { 8, 16, 1, 2, 4 },
};
static const uint8_t k_code9[7][9] = {                                   /* 0x455154 */
    { 8, 16, 8, 16, 8, 16, 8, 16, 4 },
    { 2, 1, 16, 8, 2, 1, 16, 8, 4 },
    { 8, 16, 8, 16, 1, 2, 1, 2, 4 },
    { 8, 1, 16, 2, 2, 8, 16, 1, 4 },
    { 2, 1, 16, 8, 2, 16, 1, 8, 4 },
    { 16, 16, 1, 8, 2, 16, 8, 16, 16 },
    { 1, 16, 1, 16, 2, 8, 2, 8, 4 },
};

static void clearCodeBuffers(int p) {
    memset(g_buf9[p], 0, sizeof(g_buf9[p]));
    memset(g_buf5[p], 0, sizeof(g_buf5[p]));
    memset(g_buf6[p], 0, sizeof(g_buf6[p]));
}

/* 0x415CFC / 0x415D88 / 0x415DD8: desloca e acrescenta no fim */
static void pushCode(int p, uint8_t v) {
    memmove(g_buf9[p], g_buf9[p] + 1, 8); g_buf9[p][8] = v;
    memmove(g_buf5[p], g_buf5[p] + 1, 4); g_buf5[p][4] = v;
    memmove(g_buf6[p], g_buf6[p] + 1, 5); g_buf6[p][5] = v;
}

static void unlockHidden(void);
static void playWave(int snd) {
    if (g_waveSoundIds[snd] >= 0) Audio_Play(g_waveSoundIds[snd], false);
}

/* 0x42672C: para o preview */
static void stopPreview(void) {
    BGM_Stop();
    g_previewOn = false;
}

/* 0x416BCF..0x416C09 / 0x4182D9..0x418315: "%X" -> 0x42663C monta
 * "AUDIO\D%s.AUD" e 0x426740 toca. Loop: HIPÓTESE (0x4261B0 não mostra flag). */
static void startPreview(void) {
    if (g_listCount <= 0) return;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/AUDIO/D%X.AUD", g_game.currentDirectory,
             (unsigned)g_list[wrapCursor(g_cursor[g_ch])]);
    g_previewOn = true;
    BGM_Stop();
    if (BGM_LoadAUDDirect(path)) BGM_Play(true);
    else Log_Print("EXSELECT: preview '%s' não abriu\n", path);
}

/* 0x4173E4 / 0x417611: som do DL/DR — 10-2 depois de 4 repetições, senão 3-2 */
static void playMoveSound(void) {
    playWave(g_repeat > 4 ? SND_10_2 : SND_3_2);
}

/* 0x4155AC (P1) / 0x415958 (P2): compara os três buffers com as tabelas.
 * Se o jogador ou o [0x568FF4] mudarem, toca 2-1.WAV (0x415814..0x415852). */
static void checkCodes(int p) {
    unsigned oldMods = g_mods[p], oldFlags = g_flags;
    unsigned m = g_mods[p];

    for (int k = 0; k < 7; k++) {
        if (memcmp(g_buf9[p], k_code9[k], 9) != 0) continue;
        switch (k) {                                     /* 0x41585D */
        case 0: m = (m & ~0x0Eu) ^ EXMOD_RV; break;      /* 0x415939 */
        case 1: m ^= EXMOD_M; break;                     /* 0x41592A */
        case 2: m = (m & ~EXMOD_200) ^ EXMOD_R; break;   /* 0x415912 */
        case 3: m ^= EXMOD_800; break;                   /* 0x415900 */
        case 4: m = (m & ~0x41Eu) ^ EXMOD_1000; break;   /* 0x4158BD */
        case 5: m |= EXMOD_UNLOCK; g_mods[p] = m; unlockHidden(); break; /* 0x41588F */
        case 6: g_flags ^= EX_XMODE; break;              /* 0x4158AD: X-MODE */
        }
        g_mods[p] = m;
        clearCodeBuffers(p);
        break;
    }

    for (int k = 0; k < 2; k++) {
        if (memcmp(g_buf5[p], k_code5[k], 5) != 0) continue;
        if (k == 0) {
            /* 0x41569F..0x415725: x1 -> x2 -> x3 -> x4 -> x8 -> rv -> x1 */
            unsigned c = m & ~0x41Eu;
            if      (m & EXMOD_RV) m = c;
            else if (m & EXMOD_X8) m = c ^ EXMOD_RV;
            else if (m & EXMOD_X4) m = c ^ EXMOD_X8;
            else if (m & EXMOD_X3) m = c ^ EXMOD_X4;
            else if (m & EXMOD_X2) m = c ^ EXMOD_X3;
            else                   m = c ^ EXMOD_X2;
        } else {
            /* 0x415732..0x41575A: 0 -> v -> ns -> v+ns -> 0 */
            if (m & EXMOD_V) m = (m & ~0x220u) ^ EXMOD_NS;
            else             m = (m & ~EXMOD_200) ^ EXMOD_V;
        }
        g_mods[p] = m;
        clearCodeBuffers(p);
    }

    /* 0x41579B..0x415810: DL DR DL DR DL DR zera tudo (inclusive o X-MODE) */
    if (memcmp(g_buf6[p], k_code6, 6) == 0) {
        g_flags = 0;
        g_mods[p] = 0;
        clearCodeBuffers(p);
    }

    if (g_mods[p] != oldMods || g_flags != oldFlags) {
        playWave(SND_2_1);
        Log_Print("EXSELECT: P%d mods 0x%04X, X-MODE %s\n", p + 1, g_mods[p],
                  (g_flags & EX_XMODE) ? "on" : "off");
    }
}

/* 0x4551C8 (bit do modo) e 0x4551B4 (slot do ícone no SELECT2), mesma ordem de
 * ExceedSong.level: NORMAL HARD CRAZY FREESTYLE NIGHTMARE */
/* índice 5 = BATTLE: 0x4551A4 (bits N H C BATTLE) / 0x455194 (slot 49) */
#define EX_MODE_BATTLE 5
static const int k_modeBit[6]  = { 0x10, 0x20, 0x400, 0x200, 0x800, 0x40 };
static const int k_modeIcon[6] = { 44, 45, 46, 47, 48, 49 };
/* sufixo do comando RUN (0x1283C44..0x1283C74) por modo */
static const char* k_modeArg[5] = { "-n", "-h", "-c", "-d", "-nm" };
/* quadro do fundo do painel por índice: parado / entrando pela esquerda / pela direita */
static const int k_panelIdle[5]  = { 660, 720, 780, 840, 900 };   /* 0x456F08 */
static const int k_panelDir1[5]  = { 1080, 1020, 960, 900, 900 }; /* 0x456EE0 */
static const int k_panelDir2[5]  = { 660, 660, 720, 780, 840 };   /* 0x456EF4 */

/* 0x456EC8 / 0x456ED4 */
static const int k_chLabelFrame[EX_CHANNEL_COUNT] = { 60, 120, 180 };
static const int k_chLabelSlot[EX_CHANNEL_COUNT]  = { 19, 20, 21 };

/* 0x415434: índice do registro pelo ID, -1 se não existe */
static int findSong(int id) {
    for (int i = 0; i < EX_SONG_COUNT; i++)
        if ((int)g_exSongs[i].id == id) return i;
    return -1;
}

/* 0x4192F0: monta a lista visível do canal. Música oculta (+0x35) entra se o
 * P1 ou o P2 tiver o bit 0x2000 (0x419376..0x4193C4). */
static void buildList(void) {
    bool unlocked = ((g_mods[0] | g_mods[1]) & EXMOD_UNLOCK) != 0;
    g_listCount = 0;
    for (int i = 0; i < EX_CHANNEL_MAX; i++) {
        int id = g_exChannels[g_ch][i];
        if (id == 0) break;
        int s = findSong(id);
        if (s < 0) continue;
        if (g_exSongs[s].hidden == 0 || unlocked)   /* visible = (hidden == 0), 0x416474 */
            g_list[g_listCount++] = id;
    }
}

/* 0x419424: depois do código de desbloqueio vai para o BANYA com o cursor
 * em A03 (Monkey Fingers) e remonta a lista. */
static void unlockHidden(void) {
    g_ch = 0;
    buildList();
    for (int i = 0; i < g_listCount; i++) {
        if (g_list[i] == 0xA03) {
            g_cursor[0] = i;
            g_cursorPrev[0] = i;
            break;
        }
    }
}

/* 0x419D38: cursor circular */
static int wrapCursor(int c) {
    if (g_listCount <= 0) return 0;
    while (c < 0) c += g_listCount;
    while (c >= g_listCount) c -= g_listCount;
    return c;
}

static int bannerForId(int id) {
    int s = findSong(id);
    return (s >= 0) ? g_bannerTex[s] : -1;
}

void ExSelect_Enter(void) {
    /* 0x4161E6 / 0x4161F6: os dois BGAs ficam carregados juntos */
    Resource_ClearBGA();
    if (!Resource_LoadBGAByName("SELECT"))
        Log_Print("EXSELECT: falha ao carregar BGA\\SELECT.DAT\n");
    if (!Resource_LoadBGAByName("SELECT2"))
        Log_Print("EXSELECT: falha ao carregar BGA\\SELECT2.DAT\n");
    BGA_Reset();

    /* 0x416206..0x41624F: banners do 90.DAT */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/BGA/90.DAT", g_game.currentDirectory);
    for (int i = 0; i < EX_SONG_COUNT; i++) g_bannerTex[i] = -1;
    if (RES_Open(path)) {
        int ok = 0;
        for (int i = 0; i < EX_SONG_COUNT; i++) {
            char name[16];
            /* A26/A27 usam o disco da 401/402 (Song_DataId) */
            snprintf(name, sizeof(name), "%X.TGA", (unsigned)Song_DataId((int)g_exSongs[i].id));
            g_bannerTex[i] = loadTextureFromRES(name);
            if (g_bannerTex[i] >= 0) ok++;
        }
        RES_Close();
        Log_Print("EXSELECT: %d/%d banners do 90.DAT\n", ok, EX_SONG_COUNT);
    } else {
        Log_Print("EXSELECT: falha ao abrir '%s'\n", path);
    }

    /* 0x404540..0x404570: dígitos do font.tga do BGA\00.DAT */
    snprintf(path, sizeof(path), "%s/BGA/00.DAT", g_game.currentDirectory);
    g_fontTex = -1;
    if (RES_Open(path)) {
        g_fontTex = loadTextureFromRES("font.tga");
        RES_Close();
    }
    if (g_fontTex < 0) Log_Print("EXSELECT: font.tga do 00.DAT não carregou\n");

    /* 0x41E5D7..0x41E61B: dígitos do contador (font4.tga do BGA\BFONT.DAT) */
    snprintf(path, sizeof(path), "%s/BGA/BFONT.DAT", g_game.currentDirectory);
    g_timerTex = -1;
    if (RES_Open(path)) {
        g_timerTex = loadTextureFromRES("font4.tga");
        RES_Close();
    }
    if (g_timerTex < 0) Log_Print("EXSELECT: font4.tga do BFONT.DAT não carregou\n");

    g_startTick = timeGetTime();
    g_timeLeft = 60;
    g_started = false;

    g_joined = Title_GetJoinedMask() & 3;
    if (g_joined == 0) g_joined = 1;   /* sem entrada registrada no CREDIT: P1 */
    /* 0x4162AC: [0x568FF4] &= 0xFFFFF00F — o X-MODE (0x8000) continua.
     * O +0x184 dos jogadores NÃO é zerado aqui (só pelo código
     * DL DR DL DR DL DR, 0x4157E9/0x415B99): os modificadores valem para
     * as músicas seguintes. Zera só no início de um jogo novo
     * (Menu_ResetState deixa stageCount = 3). */
    bool newGame = (g_game.stageCount == 3 && !g_game.isBonusSong);
    if (newGame) g_flags = 0;
    else         g_flags &= 0xFFFFF00Fu;
    for (int p = 0; p < 2; p++) {
        if (newGame) g_mods[p] = 0;
        g_joinFrame[p] = 0;
        clearCodeBuffers(p);
    }
    g_repeat = 0;
    g_previewOn = false;
    g_chosen = false;
    g_armed = false;
    g_panelFrame = 0;
    g_panelIdx = -1;
    g_panelDir = 0;
    g_modeCount = 0;

    /* 0x4163FC..0x41642B: estado inicial */
    g_intro = true;
    g_frame = 0;
    g_chFrame = 0;
    g_curFrame = 0;
    /* 0x4163FC: o Begin só copia o canal atual para +0x60 e zera direções;
     * canal (+0x5C) e cursores (+0x68) persistem entre as músicas. Zera
     * apenas num jogo novo. */
    if (newGame) {
        g_ch = 0;
        for (int c = 0; c < EX_CHANNEL_COUNT; c++) g_cursor[c] = 0;
    }
    g_chPrev = g_ch;
    g_chDir = 0;
    g_curDir = 0;
    for (int c = 0; c < EX_CHANNEL_COUNT; c++)
        g_cursorPrev[c] = g_cursor[c];
    buildList();
}

/* 0x417F87 (dir 1) / 0x41803E (dir 2) */
static void changeChannel(int dir) {
    playWave(SND_CHGMOD);   /* 0x417114 / 0x4171F0 */
    stopPreview();          /* 0x417F8E: 0x42672C, +0x84 = 0 */
    g_chPrev = g_ch;
    g_chDir = dir;
    g_chFrame = 0;
    if (dir == 1) g_ch = (g_ch == 0) ? 2 : g_ch - 1;
    else          g_ch = (g_ch == 2) ? 0 : g_ch + 1;
    buildList();
    g_cursor[g_ch] = wrapCursor(g_cursor[g_ch]);
    g_cursorPrev[g_ch] = g_cursor[g_ch];
}

/* 0x4180E1 (dir 1) / 0x41823C (dir 2) */
static void moveCursor(int dir) {
    playMoveSound();
    stopPreview();          /* 0x4180E8 / 0x418243 */
    g_repeat++;             /* 0x4182AE */
    g_curFrame = 0;
    g_curDir = dir;
    g_cursorPrev[g_ch] = g_cursor[g_ch];
    g_cursor[g_ch] = wrapCursor(g_cursor[g_ch] + (dir == 1 ? -1 : 1));
}

/* 0x4170D2 etc.: só os painéis de quem entrou ([0x568FF4] bits 0/1) contam */
static bool padHit(PadButton b) {
    return ((g_joined & 1) && Input_IsPadHit(0, b)) || ((g_joined & 2) && Input_IsPadHit(1, b));
}
static bool padDown(PadButton b) {
    return ((g_joined & 1) && Input_IsPadDown(0, b)) || ((g_joined & 2) && Input_IsPadDown(1, b));
}

/* 0x4182C5..0x418506: CENTER na música abre o painel. Máscara de modos =
 * níveis != -1 (0x455200 + 4*i). O bit 0x40 (BATTLE, com os dois jogadores)
 * não entra: só o caminho de 1 jogador está reproduzido. */
static void openPanel(void) {
    int s = findSong(g_list[wrapCursor(g_cursor[g_ch])]);
    if (s < 0) return;
    g_modeCount = 0;
    g_panel2P = (g_joined & 3) == 3;
    if (!g_panel2P) {
        for (int m = 0; m < 5; m++)
            if (g_exSongs[s].level[m] != -1)
                g_modeList[g_modeCount++] = m;   /* índice em k_modeBit */
    } else {
        /* 0x418339..0x4184C5: N H C disponíveis; +0x8C fica com o último
         * deles; BATTLE (0x40) entra com os dois jogadores */
        g_battleDiff = 0;
        for (int m = 0; m < 3; m++)
            if (g_exSongs[s].level[m] != -1) {
                g_modeList[g_modeCount++] = m;
                g_battleDiff = m;
            }
        g_modeList[g_modeCount++] = EX_MODE_BATTLE;
    }
    if (g_modeCount == 0) return;
    if (!g_previewOn) startPreview();   /* 0x4182CE: preview ainda não tinha disparado */
    g_chosen = true;
    g_panelFrame = 0;   /* +0x44 */
    g_panelIdx = 0;     /* +0x88 */
    g_panelDir = 0;     /* +0x58 */
}

/* 0x415514: nível da música no modo; a linha BATTLE mostra o da +0x8C */
static int levelOf(int s, int m) {
    if (m == EX_MODE_BATTLE) m = g_battleDiff;
    return g_exSongs[s].level[m];
}

/* 0x41816A..0x41822D: no painel de 2 jogadores, DR sem linha à direita troca
 * a dificuldade do BATTLE entre as disponíveis */
static void cycleBattleDiff(int s) {
    const int* lv = g_exSongs[s].level;
    if (g_battleDiff == 1) {
        if (lv[0] != -1) g_battleDiff = 0;
        else if (lv[2] != -1) g_battleDiff = 2;
    } else if (g_battleDiff == 2) {
        if (lv[1] != -1) g_battleDiff = 1;
        else if (lv[0] != -1) g_battleDiff = 0;
    } else {
        if (lv[2] != -1) g_battleDiff = 2;
        else if (lv[1] != -1) g_battleDiff = 1;
    }
}

/* 0x417153..0x41718E: UL/UR com o painel aberto cancelam a escolha */
static void cancelPanel(void) {
    playWave(SND_CHGMOD);   /* 0x417114: o som toca antes do teste de +0x95 */
    g_armed = false;
    g_chosen = false;
    g_chFrame = 0;
    g_panelIdx = -1;
}

/* 0x41850B..0x418693: início do jogo — pelo segundo CENTER no painel ou pelo
 * fim do contador. Com o painel nunca aberto/cancelado ([+0x88] == -1) usa o
 * primeiro modo disponível na ordem N H C FS NM (0x4187A9..0x4189AC).
 * Monta "RUN %X -n|-h|-c|-d|-nm" para 0x4102D4. O gameplay do Exceed ainda
 * não existe no projeto: por enquanto só registra o comando e trava a tela. */
static void startGame(void) {
    if (g_started) return;
    int s = findSong(g_list[wrapCursor(g_cursor[g_ch])]);
    if (s < 0) return;
    int m = -1;
    if (g_chosen && g_panelIdx >= 0 && g_panelIdx < g_modeCount) {
        m = g_modeList[g_panelIdx];
    } else {
        int last = ((g_joined & 3) == 3) ? 3 : 5;   /* 2P: só N H C */
        for (int k = 0; k < last; k++)
            if (g_exSongs[s].level[k] != -1) { m = k; break; }
    }
    if (m < 0) return;
    g_started = true;
    stopPreview();          /* 0x418512 */
    playWave(SND_START);    /* 0x418521 */
    /* 0x4186D4..0x418712: BATTLE usa o sufixo da dificuldade +0x8C */
    int argIdx = (m == EX_MODE_BATTLE) ? g_battleDiff : m;
    Log_Print("EXSELECT: RUN %X %s%s (modo 0x%X, nível %d)\n",
              (unsigned)g_exSongs[s].id, k_modeArg[argIdx],
              (m == EX_MODE_BATTLE) ? " [BATTLE]" : "", k_modeBit[m],
              levelOf(s, m));
    g_armed = false;

    /* 0x4102D4 ("RUN ...") -> gameplay: aproveita o fluxo Loading/Gameplay do
     * projeto via songDB sintético. */
    int id = (int)g_exSongs[s].id;
    g_game.selectedSongIndex = Song_FindByID(&g_game.songDB, id);
    g_game.selectedModeIndex = Song_FindMode(&g_game.songDB, k_dbModeName[argIdx]);
    if (g_game.selectedSongIndex < 0 || g_game.selectedModeIndex < 0) {
        Log_Print("EXSELECT: songDB sem a música/modo (%d/%d) — sem gameplay\n",
                  g_game.selectedSongIndex, g_game.selectedModeIndex);
        return;
    }
    g_game.selectedDifficulty = levelOf(s, m);
    g_game.activePlayerMask = (int)(g_joined & 3);
    g_game.isBattleMode = (m == EX_MODE_BATTLE);
    /* Velocidade confirmada pelos ícones (x2/x3/x4/x8); rv entra no mesmo
     * ciclo de velocidade. Os demais (r/m/v/ns, 0x800, 0x1000, X-MODE) ainda
     * não têm efeito no gameplay do projeto. */
    for (int p = 0; p < 2; p++) {
        unsigned mm = g_mods[p];
        g_game.cmdSpeedMult[p] = (mm & EXMOD_X8) ? 8 : (mm & EXMOD_X4) ? 4 :
                                 (mm & EXMOD_X3) ? 3 : (mm & EXMOD_X2) ? 2 : 1;
        g_game.cmdRandomVelocity[p] = (mm & EXMOD_RV) != 0;
        g_game.cmdRandomStep[p] = (mm & EXMOD_R) != 0;
        g_game.cmdMirror[p] = (mm & EXMOD_M) != 0;
        g_game.cmdVanish[p] = (mm & EXMOD_V) != 0;
        g_game.cmdNonStep[p] = (mm & EXMOD_NS) != 0;
    }
    if (argIdx == 3 || argIdx == 4) {
        int pSrc = (((g_joined & 2) && !(g_joined & 1)) || (g_mods[1] != 0 && g_mods[0] == 0)) ? 1 : 0;
        g_game.cmdSpeedMult[0] = g_game.cmdSpeedMult[1] = g_game.cmdSpeedMult[pSrc];
        g_game.cmdRandomVelocity[0] = g_game.cmdRandomVelocity[1] = g_game.cmdRandomVelocity[pSrc];
        g_game.cmdRandomStep[0] = g_game.cmdRandomStep[1] = g_game.cmdRandomStep[pSrc];
        g_game.cmdMirror[0] = g_game.cmdMirror[1] = g_game.cmdMirror[pSrc];
        g_game.cmdVanish[0] = g_game.cmdVanish[1] = g_game.cmdVanish[pSrc];
        g_game.cmdNonStep[0] = g_game.cmdNonStep[1] = g_game.cmdNonStep[pSrc];
    }
    Loading_Enter(id);
}

/* Comando RUN/PLAY do console (parser 0x401A0A..0x401D56): "<id> <modo> [-demo|-demo2]".
 * Entra direto no gameplay, sem passar pela Select. m = índice de k_dbModeName ou
 * EX_MODE_BATTLE (-bt), que usa a última dificuldade disponível entre N/H/C.
 * [0x568FF4] começa em 3 (os dois jogadores); -demo liga o autoplay da atração. */
bool ExSelect_Run(int id, int m, int demo)
{
    int s = findSong(id);
    if (s < 0) return false;
    int argIdx = m;
    if (m == EX_MODE_BATTLE) {
        argIdx = -1;
        for (int k = 0; k < 3; k++)
            if (g_exSongs[s].level[k] != -1) argIdx = k;
        if (argIdx < 0) return false;
        g_battleDiff = argIdx;
    }
    if (argIdx < 0 || argIdx > 4 || g_exSongs[s].level[argIdx] == -1) return false;

    g_game.selectedSongIndex = Song_FindByID(&g_game.songDB, id);
    g_game.selectedModeIndex = Song_FindMode(&g_game.songDB, k_dbModeName[argIdx]);
    if (g_game.selectedSongIndex < 0 || g_game.selectedModeIndex < 0) return false;

    Menu_ResetState();
    g_game.selectedDifficulty = g_exSongs[s].level[argIdx];
    g_game.activePlayerMask = (argIdx == 3 || argIdx == 4 || demo == 2) ? 1 : 3;
    g_game.isBattleMode = (m == EX_MODE_BATTLE);
    for (int p = 0; p < 2; p++) {
        g_game.cmdSpeedMult[p] = 1;
        g_game.cmdRandomVelocity[p] = false;
    }
    g_exDemo = (demo != 0);
    Log_Print("RUN %X %s%s%s\n", (unsigned)id, k_modeArg[argIdx],
              (m == EX_MODE_BATTLE) ? " -bt" : "",
              demo == 2 ? " -demo2" : demo ? " -demo" : "");
    Loading_Enter(id);
    return true;
}

/* 0x41944C: com crédito, CENTER de quem ainda não entrou -> PUSHPANEL.WAV,
 * consome o crédito, liga o bit do jogador e fecha o painel (+0x95 = 0).
 * Testa o P1 antes; se o P1 entrou neste quadro, o P2 fica para o próximo. */
static void tryLateJoin(void) {
    if (!Coin_HasCredit()) return;
    for (int p = 0; p < 2; p++) {
        if (g_joined & (1u << p)) continue;
        if (!Input_IsPadHit(p, PAD_C)) continue;
        playWave(SND_PUSHPANEL);
        g_joinFrame[p] = 0;
        Coin_ConsumeCredit();
        g_joined |= (1u << p);
        g_chosen = false;
        Log_Print("EXSELECT: P%d entrou na Select\n", p + 1);
        return;
    }
}

void ExSelect_Update(float dt) {
    (void)dt;

    if (g_intro) {
        /* 0x4165A4: +0x2C, +0x4C e +0x54 andam; com +0x2C >= 30 vai para o
         * estado principal e zera o cronômetro (0x426940). */
        g_frame++;
        g_joinFrame[0]++;
        g_joinFrame[1]++;
        if (g_frame >= 0x1E) {
            g_intro = false;
            g_startTick = timeGetTime();
        }
        return;
    }

    if (g_started) {
        /* RUN já emitido: sem gameplay ainda, só segue animando */
        g_frame++;
        g_chFrame++;
        g_curFrame++;
        g_joinFrame[0]++;
        g_joinFrame[1]++;
        return;
    }

    if (!g_chosen) {
        /* 0x4170D2..0x4171EA: UL/UR (bits 0x08/0x10) trocam o canal; segurando,
         * repete depois que a rotação de 30 quadros termina. */
        if (padHit(PAD_UL) || (padDown(PAD_UL) && g_chFrame >= 30))
            changeChannel(1);
        else if (padHit(PAD_UR) || (padDown(PAD_UR) && g_chFrame >= 30))
            changeChannel(2);
        /* 0x4174C0..: DL/DR (bits 0x01/0x02) movem o cursor; segurando, repete
         * com [this+0x3C] > 20. */
        else if (padHit(PAD_DL) || (padDown(PAD_DL) && g_curFrame > 20))
            moveCursor(1);
        else if (padHit(PAD_DR) || (padDown(PAD_DR) && g_curFrame > 20))
            moveCursor(2);
        /* 0x41770B..0x41775D: CENTER (bit 0x04 / 0x400) */
        else if (padHit(PAD_C))
            openPanel();
    } else {
        if (padHit(PAD_UL) || padHit(PAD_UR)) {
            cancelPanel();
        } else if (padHit(PAD_DL)) {
            /* 0x417457..0x4174AE */
            playMoveSound();
            g_armed = false;
            if (g_panelIdx > 0) {
                g_panelFrame = 60;
                g_panelIdx--;
                g_panelDir = 1;
            } else {
                g_panelDir = 0;
            }
        } else if (padHit(PAD_DR)) {
            /* 0x417684..0x4176F9 */
            playMoveSound();
            g_armed = false;
            if (g_panelIdx < g_modeCount - 1) {
                g_panelFrame = 60;
                g_panelIdx++;
                g_panelDir = 2;
            } else if (g_panel2P) {
                int s = findSong(g_list[wrapCursor(g_cursor[g_ch])]);
                if (s >= 0) cycleBattleDiff(s);   /* 0x41769E -> 0x41816A */
                g_panelDir = 0;                   /* 0x4176AF */
            } else {
                g_panelDir = 0;
            }
        } else if (padHit(PAD_C)) {
            /* 0x417763..0x417788: primeiro CENTER arma, o segundo confirma */
            if (g_armed) {
                startGame();
            } else {
                g_armed = true;
                playWave(SND_3_2);  /* 0x417794 */
            }
        }
    }

    /* 0x4177C4: entrada de jogador com crédito */
    tryLateJoin();

    /* 0x415E3C (chamado em 0x4177C9): cada painel apertado entra nos buffers
     * na ordem DL DR C UL UR; se houve toque, confere os códigos e toca 3-2. */
    static const PadButton order[5] = { PAD_DL, PAD_DR, PAD_C, PAD_UL, PAD_UR };
    static const uint8_t   bits[5]  = { 0x01, 0x02, 0x04, 0x08, 0x10 };
    for (int p = 0; p < 2; p++) {
        if (!(g_joined & (1u << p))) continue;
        bool any = false;
        for (int k = 0; k < 5; k++) {
            if (Input_IsPadHit(p, order[k])) {
                pushCode(p, bits[k]);
                any = true;
            }
        }
        if (any) {
            checkCodes(p);
            playWave(SND_3_2);   /* 0x415ECF / 0x415F17 */
        }
    }

    /* 0x4177D1: tempo esgotado (ou [+0xA0] zerado) -> início do jogo */
    if (g_timeLeft <= 0) startGame();

    /* 0x416FAD..0x41701A: 60 s pelo cronômetro; ao mudar e estar <= 5 toca
     * TIME_LIMIT.WAV */
    {
        int ms = (int)(timeGetTime() - g_startTick);
        int left = (60000 - ms) / 1000;
        if (left != g_timeLeft && left <= 5)
            playWave(SND_TIME_LIMIT);
        g_timeLeft = left;
    }

    /* 0x416B27..0x416BA8: solto o DL/DR, zera a contagem de repetição */
    if (!padDown(PAD_DL) && !padDown(PAD_DR))
        g_repeat = 0;

    /* 0x416BB2..0x416C09: cursor parado há mais de 30 quadros, sem rotação de
     * canal e sem preview -> toca AUDIO\D%X.AUD */
    if (!g_started && g_curFrame > 30 && g_chDir == 0 && !g_previewOn)
        startPreview();

    g_frame++;
    g_chFrame++;
    g_curFrame++;
    if (g_chosen) g_panelFrame++;   /* 0x41789C */
    g_joinFrame[0]++;               /* 0x4178B4 (+0x4C) */
    g_joinFrame[1]++;               /* 0x4178D6 (+0x54) */
}

/* S3DSetProjection(43.603): câmera a 600 do plano z=0, que fica 1:1 com a tela.
 * Os quads 2D do BGA (z=0) saem iguais aos do ortho. */
static void setProjection(void) {
    const float n = 10.0f, f = 4000.0f;
    const float k = n / 600.0f;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-320.0f * k, 320.0f * k, -240.0f * k, 240.0f * k, n, f);
    glTranslatef(-320.0f, -240.0f, -600.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* S3DSetOrtho */
static void setOrtho(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* Espaço 3D do S3D = o do GL do projeto: Y para cima, câmera olhando -Z
 * (confirmado visualmente em 26/09/2026). A tentativa anterior com Y para
 * baixo / +Z afastando (convenção D3D8) deixava a roda curvada para cima e o
 * banner central grande e alto demais. */
static void enterS3DSpace(void) {
    /* glTranslatef(0.0f, 480.0f, 0.0f); */
    /* glScalef(1.0f, -1.0f, -1.0f); */
}

/* Quad na ordem do original (0x41977B..0x4197EF / 0x41987A..0x4198FC):
 *   (x0,yA,zA) t(0,0)  (x0,yB,zB) t(0,1)  (x1,yB,zB) t(1,1)  (x1,yA,zA) t(1,0)
 * yA é a borda de cima (Y para cima) e recebe V=0 = topo da imagem. */
static void bannerQuad(int tex, float x0, float x1, float yA, float zA, float yB, float zB) {
    Texture_Bind(tex);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x0, yA, zA);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(x0, yB, zB);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(x1, yB, zB);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x1, yA, zA);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

/* 0x4195D8(cursor, ângulo): roda de 11 banners (i = -5..5), 27.69° entre eles.
 * Constantes: 0x456E9C..0x456EC4. tx/ty dos banners (0x563AC8/0x563ACC) só
 * mudam pelo console de debug: valem 0.
 * Todas são ajustáveis pelo /set do console (0x41D309), com estes nomes. */
float g_exCamX1 = -62.0f;   /* 0x456E9C */
float g_exCamX2 =  62.0f;   /* 0x456EA0 */
float g_exCamY1 =   0.0f;   /* 0x563ACC */
float g_exCamY2 =  90.0f;   /* 0x456EA4 */
float g_exCamZ1 =  90.0f;   /* 0x456EA8 */
float g_exCamR  = -27.69f;  /* 0x456EAC */
float g_exCamR2 =  38.0f;   /* 0x456EB0 */
float g_exCamR3 = -51.0f;   /* 0x456EB4 */
float g_exCamTx =   0.0f;   /* 0x563AC8 */
float g_exCamTy = 320.0f;   /* 0x456EB8 */
float g_exCamP  =  50.0f;   /* 0x456EBC */
float g_exCamCy =  73.0f;   /* 0x456EC0 */
float g_exCamCy2 = 66.0f;   /* 0x456EC4 */

static void drawCarousel(int cursor, float angle, float r, float g, float b, float a) {
    if (g_listCount <= 0) return;
    glPushMatrix();
    enterS3DSpace();
    glTranslatef(320.0f, g_exCamCy, g_exCamZ1);
    glTranslatef(0.0f, g_exCamCy2, 0.0f);
    glRotatef(g_exCamR3, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -g_exCamCy2, 0.0f);
    glColor4f(r, g, b, a);
    for (int i = -5; i < 6; i++) {
        int idx = cursor + i;
        if (idx < 0) idx += g_listCount;
        if (idx >= g_listCount) idx -= g_listCount;
        if (idx < 0 || idx >= g_listCount) continue;

        glPushMatrix();
        glTranslatef(0.0f, g_exCamP, 0.0f);
        glRotatef(angle, 0.0f, 0.0f, 1.0f);
        glRotatef((float)i * g_exCamR, 0.0f, 0.0f, 1.0f);
        /* 0x4196C1..0x4196DE: Push / Rotatef(90,0,0,1) / Pop — sem efeito */
        glTranslatef(0.0f, -g_exCamP, 0.0f);

        int tex = bannerForId(g_list[idx]);
        if (tex >= 0) {
            glPushMatrix();
            glTranslatef(g_exCamTx, g_exCamTy, 0.0f);
            glRotatef(g_exCamR2, 1.0f, 0.0f, 0.0f);
            bannerQuad(tex, g_exCamX1, g_exCamX2, g_exCamY2, 0.0f, g_exCamY1, 0.0f);
            glPopMatrix();
        }
        glPopMatrix();
    }
    glPopMatrix();
    glColor4f(1, 1, 1, 1);
}

/* 0x41983C: banner grande, borda de cima inclinada para trás (z = -200) */
static void drawCenterBanner(int cursor, float r, float g, float b, float a) {
    if (g_listCount <= 0) return;
    int tex = bannerForId(g_list[wrapCursor(cursor)]);
    if (tex < 0) return;
    glPushMatrix();
    enterS3DSpace();
    glColor4f(r, g, b, a);
    bannerQuad(tex, 81.0f, 559.0f, 290.0f, -200.0f, 65.0f, 0.0f);
    glPopMatrix();
    glColor4f(1, 1, 1, 1);
}

/* 0x40C55C(x, y, w, h, dígito): célula do font.tga numa grade de 8 colunas.
 * Vértices em Y para cima: topo = y + h + 8, largura w + 4. */
static void drawDigit(int x, int y, int w, int h, int d) {
    float u0 = (float)(d & 7) * 0.125f;
    float u1 = u0 + 0.125f;
    float v0 = (float)(d >> 3) * 0.12109375f + 0.28515625f;
    float v1 = v0 + 0.12109375f;
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2i(x, y + h + 8);
    glTexCoord2f(u0, v1); glVertex2i(x, y);
    glTexCoord2f(u1, v1); glVertex2i(x + w + 4, y);
    glTexCoord2f(u1, v0); glVertex2i(x + w + 4, y + h + 8);
    glEnd();
}

/* 0x40C678(x, y, w, h, passo, valor, dígitos): da direita para a esquerda */
static void drawNumber(int x, int y, int w, int h, int step, int value, int digits) {
    if (g_fontTex < 0) return;
    Texture_Bind(g_fontTex);
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < digits; i++) {
        drawDigit(x, y, w, h, value % 10);
        x -= step;
        value /= 10;
    }
    glDisable(GL_TEXTURE_2D);
}

/* Linha do painel: quadro (slot 34), número do nível e ícone do modo.
 * bright/alpha seguem os dois ramos de 0x4179A5 e 0x417C7C. */
static void drawPanelRow(int s, int m, bool lit, float hl, float b) {
    BGA_DrawSlot(SEL2_BGA, 1170, 34);
    if (lit) {
        BGA_SetColor(SEL2_BGA, 1.0f, hl);
        BGA_DrawSlot(SEL2_BGA, 660, 39);
        BGA_SetColor(SEL2_BGA, 1.0f, 1.0f);
        glColor4f(b, b, b, 1.0f);
        drawNumber(455, 324, 25, 25, 19, levelOf(s, m), 2);
        BGA_SetColor(SEL2_BGA, b, 1.0f);
        BGA_DrawSlot(SEL2_BGA, 1170, k_modeIcon[m]);
    } else {
        glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
        drawNumber(455, 324, 25, 25, 19, levelOf(s, m), 2);
        BGA_SetColor(SEL2_BGA, 0.5f, 1.0f);
        BGA_DrawSlot(SEL2_BGA, 1170, k_modeIcon[m]);
    }
    BGA_SetColor(SEL2_BGA, 1.0f, 1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* 0x4178EB..0x417F16: painel de dificuldade de 1 jogador (SELECT2, ortho) */
static void drawPanel(void) {
    int s = findSong(g_list[wrapCursor(g_cursor[g_ch])]);
    if (s < 0 || g_modeCount <= 0) return;
    int pf = g_panelFrame;
    int bgSlot = g_armed ? 31 : 33;

    if (pf >= 90) g_panelDir = 0;

    if (pf < 60) {
        /* entrada: as linhas deslizam da esquerda (x = 7*pf - 70, -35 por linha)
         * e acendem (alpha = 0.05*pf + 0.5, -0.25 por linha) */
        BGA_DrawSlot(SEL2_BGA, pf + 570, bgSlot);
        float x = (float)pf * 7.0f - 70.0f;
        float y = 0.0f;
        float a = (float)pf * 0.05f + 0.5f;
        for (int k = 0; k < g_modeCount; k++) {
            float ca = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
            float cx = x > 0.0f ? 0.0f : x;
            glPushMatrix();
            glTranslatef(cx, y, 0.0f);
            int m = g_modeList[k];
            BGA_SetColor(SEL2_BGA, 1.0f, ca);              /* 0x417A53 */
            BGA_DrawSlot(SEL2_BGA, 1170, 34);
            if (k == 0) {                                  /* 0x417A91 */
                BGA_DrawSlot(SEL2_BGA, 660, 39);
                glColor4f(1.0f, 1.0f, 1.0f, ca);
                drawNumber(455, 324, 25, 25, 19, levelOf(s, m), 2);
                BGA_DrawSlot(SEL2_BGA, 1170, k_modeIcon[m]);
            } else {                                       /* 0x417B0D */
                glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
                drawNumber(455, 324, 25, 25, 19, levelOf(s, m), 2);
                BGA_SetColor(SEL2_BGA, 0.5f, 1.0f);
                BGA_DrawSlot(SEL2_BGA, 1170, k_modeIcon[m]);
                BGA_SetColor(SEL2_BGA, 1.0f, 1.0f);
            }
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glPopMatrix();
            x += -35.0f;
            y += -70.0f;
            a += -0.25f;
        }
        BGA_SetColor(SEL2_BGA, 1.0f, 1.0f);
        return;
    }

    int idx = g_panelIdx;
    int frame;
    if (g_panelDir == 1)      frame = k_panelDir1[idx] + pf - 60;
    else if (g_panelDir == 2) frame = k_panelDir2[idx] + pf - 60;
    else                      frame = k_panelIdle[idx];
    BGA_DrawSlot(SEL2_BGA, frame, bgSlot);

    float t = (float)(pf - 60) / 10.0f;
    float hl = (g_panelDir == 0) ? 1.0f : (t > 1.0f ? 1.0f : t);
    float b  = (g_panelDir == 0) ? 1.0f : (0.5f + t * 0.5f > 1.0f ? 1.0f : 0.5f + t * 0.5f);
    float y = 0.0f;
    for (int k = 0; k < g_modeCount; k++) {
        glPushMatrix();
        glTranslatef(0.0f, y, 0.0f);
        drawPanelRow(s, g_modeList[k], k == idx, hl, b);
        glPopMatrix();
        y += -70.0f;
    }
}

/* 0x41EDB0(x, y, valor) + 0x41ECA0: 2 dígitos do font4.tga, grade de 7
 * colunas (u = col*0.13671875, v = 0.75 + linha*0.125), célula 35x32,
 * unidade primeiro, recuando 35 px. */
static void drawTimerNumber(int x, int y, int value) {
    if (g_timerTex < 0) return;
    Texture_Bind(g_timerTex);
    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < 2; i++) {
        int d = value % 10;
        float u0 = (float)(d % 7) * 0.13671875f;
        float u1 = u0 + 0.13671875f;
        float v0 = (float)(d / 7) * 0.125f + 0.75f;
        float v1 = v0 + 0.125f;
        glBegin(GL_QUADS);
        glTexCoord2f(u0, v0); glVertex2i(x, y + 32);
        glTexCoord2f(u0, v1); glVertex2i(x, y);
        glTexCoord2f(u1, v1); glVertex2i(x + 35, y);
        glTexCoord2f(u1, v0); glVertex2i(x + 35, y + 32);
        glEnd();
        value /= 10;
        x -= 35;
    }
    glDisable(GL_TEXTURE_2D);
}

/* 0x41701D..0x417081: contador — normal em (607,431) e de novo em (605,433)
 * com blend aditivo (S3D 6,1 = SRC_ALPHA, ONE); volta para 6,7. */
static void drawTimer(void) {
    int v = g_timeLeft < 0 ? 0 : g_timeLeft;
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    drawTimerNumber(607, 431, v);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawTimerNumber(605, 433, v);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

/* 0x419B2C / 0x41991E: ícones de modificador no SELECT2.
 * P1 = slots 11..19 (x=14), P2 = slots 1..9 (x=585). Ordem no bloco:
 * +0 rv, +1 x8, +2 x4, +3 x3, +4 x2, +5 r, +6 m, +7 v, +8 ns.
 * Ligado = cor 1.0, desligado = 0.5 (0x41F754). */
static void drawModIcons(int p, int frame) {
    int base = (p == 0) ? 11 : 1;
    unsigned m = g_mods[p];
    BGA_SetColor(SEL2_BGA, 1.0f, 1.0f);
    if      (m & EXMOD_X2) BGA_DrawSlot(SEL2_BGA, frame, base + 4);
    else if (m & EXMOD_X3) BGA_DrawSlot(SEL2_BGA, frame, base + 3);
    else if (m & EXMOD_X4) BGA_DrawSlot(SEL2_BGA, frame, base + 2);
    else if (m & EXMOD_X8) BGA_DrawSlot(SEL2_BGA, frame, base + 1);
    else if (m & EXMOD_RV) BGA_DrawSlot(SEL2_BGA, frame, base + 0);
    else {
        BGA_SetColor(SEL2_BGA, 0.5f, 1.0f);   /* sem velocidade: x2 apagado */
        BGA_DrawSlot(SEL2_BGA, frame, base + 4);
    }
    static const unsigned onOff[4] = { EXMOD_R, EXMOD_M, EXMOD_V, EXMOD_NS };
    for (int k = 0; k < 4; k++) {
        BGA_SetColor(SEL2_BGA, (m & onOff[k]) ? 1.0f : 0.5f, 1.0f);
        BGA_DrawSlot(SEL2_BGA, frame, base + 5 + k);
    }
    BGA_SetColor(SEL2_BGA, 1.0f, 1.0f);
}

/* 0x419081 (P1, +0x4C) / 0x419170 (P2, +0x54): moldura icons_s do SELECT
 * (P1 slots 33..37, P2 28..32) e os ícones; entram em 30 quadros. */
static void drawPlayerMods(int p) {
    if (!(g_joined & (1u << p))) return;
    int t = g_joinFrame[p];
    int first = (p == 0) ? 33 : 28;
    int sf = (t > 30) ? 390 : t + 360;
    for (int s = 0; s < 5; s++)
        BGA_DrawSlot(SEL_BGA, sf, first + s);
    drawModIcons(p, (t > 30) ? 30 : t);
}

void ExSelect_Render(void) {
    if (g_game.bgaPicCount <= 0) return;
    /* [this+0x90] (0x416965..0x4169C4): 1.0 sem escolha; com o painel aberto
     * cai para 0.5 em 10 quadros. Vai para o SELECT via 0x41F754 e para a cor
     * dos banners. */
    float c = 1.0f;
    if (g_chosen) c = (g_panelFrame > 10) ? 0.5f : (float)g_panelFrame / -20.0f + 1.0f;
    BGA_SetColor(SEL_BGA, c, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (g_intro) {
        /* 0x4165CB: SELECT inteiro (0x41F520) no quadro +0x2C — é onde entram
         * X.spr / xs.spr (X00..X24). Depois P2 (bit 2) e P1 (bit 1):
         * moldura no quadro t+360 e ícones (0x419910 / 0x419B24). */
        BGA_SetEventFrame(SEL_BGA, g_frame);
        drawPlayerMods(1);
        drawPlayerMods(0);
        return;
    }

    setProjection();

    BGA_DrawSlot(SEL_BGA, g_frame % 240, 0);
    BGA_DrawSlot(SEL_BGA, g_frame % 240, 1);
    BGA_DrawSlot(SEL_BGA, g_frame % 240, 2);
    BGA_DrawSlot(SEL_BGA, 30, 14);
    BGA_DrawSlot(SEL_BGA, 30, 18);
    BGA_DrawSlot(SEL_BGA, (g_chFrame % 180) + 60, 15 + g_ch);

    /* 0x4168D8..0x416960: rótulo do canal */
    int lf = k_chLabelFrame[g_ch], ls = k_chLabelSlot[g_ch];
    if (g_chFrame < 30) {
        BGA_DrawSlot(SEL_BGA, lf + (g_chDir == 0 ? 30 : g_chFrame), ls);
        if (g_ch == g_chPrev)
            BGA_DrawSlot(SEL_BGA, lf, ls);
    } else {
        BGA_DrawSlot(SEL_BGA, lf + 30, ls);
    }

    /* 0x4169F5..0x416B20: carrossel */
    int cur = g_cursor[g_ch];
    if (g_curFrame < 10) {
        if (cur != g_cursorPrev[g_ch]) {
            if (g_curDir == 1) {
                drawCarousel(cur + 1, (float)g_curFrame * -2.769f, c, c, c, 1.0f);
                BGA_DrawSlot(SEL_BGA, g_curFrame + 360, 4);
            } else if (g_curDir == 2) {
                drawCarousel(cur - 1, (float)g_curFrame * 2.769f, c, c, c, 1.0f);
                BGA_DrawSlot(SEL_BGA, g_curFrame + 480, 5);
            }
        } else {
            drawCarousel(cur, 0.0f, c, c, c, (float)g_curFrame / 10.0f);
            BGA_DrawSlot(SEL_BGA, 30, 3);
        }
    } else {
        drawCarousel(cur, 0.0f, c, c, c, 1.0f);
        BGA_DrawSlot(SEL_BGA, 30, 3);
    }

    /* 0x416C0E..0x416D31: rotação do canal ou fundo parado */
    if (g_chFrame < 30) {
        if (g_chPrev != g_ch) {
            if (g_chDir == 1) {
                BGA_DrawSlot(SEL_BGA, g_chFrame + 480, 6);
                BGA_DrawSlot(SEL_BGA, g_chFrame + 480, 7);
                BGA_DrawSlot(SEL_BGA, g_chFrame + 480, 10);
                BGA_DrawSlot(SEL_BGA, g_chFrame + 480, 11);
            } else if (g_chDir == 2) {
                BGA_DrawSlot(SEL_BGA, g_chFrame + 360, 8);
                BGA_DrawSlot(SEL_BGA, g_chFrame + 360, 9);
                BGA_DrawSlot(SEL_BGA, g_chFrame + 360, 12);
                BGA_DrawSlot(SEL_BGA, g_chFrame + 360, 13);
            } else {
                BGA_DrawSlot(SEL_BGA, 30, 24);
            }
        } else {
            g_chDir = 0;
            BGA_DrawSlot(SEL_BGA, 30, 24);
        }
    } else {
        BGA_DrawSlot(SEL_BGA, 30, 24);
        g_chDir = 0;
    }

    /* 0x416D38..0x416E2B: banner central (cross-fade na troca de cursor) */
    if (g_curFrame < 10) {
        if (g_cursorPrev[g_ch] != cur) {
            drawCenterBanner(cur, c, c, c, 1.0f);
            drawCenterBanner(g_cursorPrev[g_ch], c, c, c, (float)g_curFrame / -10.0f + 1.0f);
        } else {
            drawCenterBanner(cur, c, c, c, (float)g_curFrame / 10.0f);
        }
    } else {
        drawCenterBanner(cur, c, c, c, 1.0f);
        g_curDir = 0;
    }

    setOrtho();

    BGA_DrawSlot(SEL_BGA, g_chFrame % 240, 25);
    BGA_DrawSlot(SEL_BGA, g_chFrame % 240, 26);
    BGA_DrawSlot(SEL_BGA, g_chFrame % 240, 27);
    BGA_DrawSlot(SEL_BGA, 30, 49);
    if (g_frame > 100) {
        BGA_DrawSlot(SEL_BGA, (g_frame - 100) % 120 + 100, 43);
        BGA_DrawSlot(SEL_BGA, (g_frame - 100) % 120 + 100, 44);
    } else {
        BGA_DrawSlot(SEL_BGA, g_frame, 43);
        BGA_DrawSlot(SEL_BGA, g_frame, 44);
    }

    /* 0x416F95: P2 (0x419170) e P1 (0x419081) */
    drawPlayerMods(1);
    drawPlayerMods(0);

    /* 0x416FAA */
    drawTimer();

    /* 0x4170C0: com a música escolhida o quadro segue para o painel */
    if (g_chosen) drawPanel();
}
