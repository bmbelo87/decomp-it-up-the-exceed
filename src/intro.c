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
        }
        Movie_Update(dt);
        break;
    default:
        break;
    }
}

void Gamestate_RenderIntro(void) {
    Movie_Render();
}
