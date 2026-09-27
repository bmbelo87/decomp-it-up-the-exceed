/*
 * intro.c — telas de vídeo do Exceed.
 *   STATE_INTRO  (CIntro::Begin): BGA\INTRO.MOV + AUDIO\001.AUD, depois do logo 81.
 *   STATE_CREDIT (CTitle::Begin): CREDIT.MOV (raiz do jogo), em loop.
 *
 * Ordem das telas informada pelo usuário: R_WARN_A -> 81 -> INTRO -> CREDIT.
 * Hipótese ainda não conferida no exceed.exe: CENTER pula o INTRO.
 */
#include "pumpy.h"
#include "movie.h"
#include "bga.h"

static unsigned g_titleJoined;   /* [0x568FF4]: bit0 = P1 entrou, bit1 = P2 */
static int      g_titleFade;     /* [CTitle+0x1C] */

/* [0x568FF4] bits 0/1: quem entrou no CREDIT (lido pela Select) */
unsigned Title_GetJoinedMask(void) { return g_titleJoined; }

static void intro_open(const char* rel, bool loop) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", g_game.currentDirectory, rel);
    Movie_Open(path, loop);
}

void Gamestate_UpdateIntro(float dt) {
    switch (g_game.state) {
    case STATE_INTRO:
        if (g_game.stateFrame == 1) {
            char aud[MAX_PATH];
            snprintf(aud, sizeof(aud), "%s/AUDIO/001.AUD", g_game.currentDirectory);
            intro_open("BGA/INTRO.MOV", false);
            BGM_Stop();
            if (BGM_LoadAUDDirect(aud)) BGM_Play(false);
        }
        Movie_Update(dt);
        if (g_game.stateFrame > 1 &&
            (Movie_HasEnded() || !Movie_IsOpen() ||
             Input_IsPadHit(0, PAD_C) || Input_IsPadHit(1, PAD_C))) {
            Log_Print("INTRO: fim (frame %d, ended=%d open=%d)\n", g_game.stateFrame,
                      Movie_HasEnded(), Movie_IsOpen());
            Movie_Close();
            Game_ChangeState(STATE_CREDIT);
        }
        break;
    case STATE_CREDIT:
        if (g_game.stateFrame == 1) {
            /* CTitle::Begin (0x41C05A): 0x426570("AUDIO\TITLE.AUD") e depois
             * 0x4229C8("CREDIT.MOV", 1) — vídeo em loop. */
            char aud[MAX_PATH];
            snprintf(aud, sizeof(aud), "%s/AUDIO/TITLE.AUD", g_game.currentDirectory);
            BGM_Stop();
            if (BGM_LoadAUDDirect(aud)) BGM_Play(true); /* loop do BGM: hipótese */
            intro_open("CREDIT.MOV", true);
            g_titleJoined = 0;
            g_titleFade = 0;
        }
        Movie_Update(dt);

        /* 0x41C192..0x41C1C8 / 0x41C594 / 0x41C5EC: com crédito, CENTER do
         * P1 (bit 0x4 de [0x568FE8]) ou do P2 (bit 0x400) entra o jogador:
         * consome crédito (0x41FEE0), liga o bit em [0x568FF4] e toca
         * WAVE/2-1.wav ([0xA6D304], carregado em 0x4258A6). */
        if (Coin_HasCredit()) {
            if (!(g_titleJoined & 1) && Input_IsPadHit(0, PAD_C)) {
                Coin_ConsumeCredit();
                g_titleJoined |= 1;
                Audio_Play(g_waveSoundIds[SND_2_1], false);
            } else if (!(g_titleJoined & 2) && Input_IsPadHit(1, PAD_C)) {
                Coin_ConsumeCredit();
                g_titleJoined |= 2;
                Audio_Play(g_waveSoundIds[SND_2_1], false);
            }
        }

        /* 0x41C4F3 / 0x41C56B: com alguém dentro, [this+0x1C] conta +1 por
         * frame (fade preto = contador/60) e passando de 60 vai para "SELECT"
         * (0x4102D4). */
        if (g_titleJoined & 3) {
            if (g_titleFade > 60) {
                Movie_Close();
                /* Game_ChangeState(STATE_SONG_SELECT); */  /* select do Prex3 (099.DAT) */
                Game_ChangeState(STATE_EXSELECT);
                return;
            }
            g_titleFade++;
        }
        break;
    default:
        break;
    }
}

/* CTitle: desenho do BGA\82.DAT sobre o vídeo (0x41C1CE..0x41C483).
 *   com crédito (0x41FEB8 != 0): slots 16,17,20 no quadro 244 + c%30 (P1)
 *                                 e 365 + c%30 (P2)  — bb, ba, pcs
 *   sem crédito:                  slots 6,7 no quadro c%47 (P1)
 *                                 e 147 + c%47 (P2)   — pic
 *   P1/P2 pulados pelos bits 0/1 de [0x568FF4]; sempre slot 3 (ltd) no quadro 0.
 * c = [CTitle+0x14], +1 por frame (0x41C54E) = stateFrame. */
void Gamestate_RenderIntro(void) {
    Movie_Render();
    if (g_game.state != STATE_CREDIT || g_game.bgaPicCount <= 0) return;

    int c = g_game.stateFrame;
    unsigned joined = g_titleJoined;           /* [0x568FF4] */
    if (Coin_HasCredit()) {
        if (!(joined & 1)) {
            BGA_DrawSlot(0, 244 + c % 30, 16);
            BGA_DrawSlot(0, 244 + c % 30, 17);
            BGA_DrawSlot(0, 244 + c % 30, 20);
        }
        if (!(joined & 2)) {
            BGA_DrawSlot(0, 365 + c % 30, 16);
            BGA_DrawSlot(0, 365 + c % 30, 17);
            BGA_DrawSlot(0, 365 + c % 30, 20);
        }
    } else {
        if (!(joined & 1)) {
            BGA_DrawSlot(0, c % 47, 6);
            BGA_DrawSlot(0, c % 47, 7);
        }
        if (!(joined & 2)) {
            BGA_DrawSlot(0, 147 + c % 47, 6);
            BGA_DrawSlot(0, 147 + c % 47, 7);
        }
    }
    BGA_DrawSlot(0, 0, 3);

    /* 0x41C488: fade preto por cima, alpha = [CTitle+0x1C] / 60.0 */
    if (g_titleFade > 0) {
        float a = (float)g_titleFade / 60.0f;
        if (a > 1.0f) a = 1.0f;
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.0f, 0.0f, a);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(640, 0); glVertex2f(640, 480); glVertex2f(0, 480);
        glEnd();
        glColor4f(1, 1, 1, 1);
    }
}
