#include "pumpy.h"

static bool padHit(int player, PadButton btn) {
    return Input_IsPadHit(player, btn);
}

static void subEnter(GameState state) {
    g_game.stateFrame = 0;
    g_game.state = state;
}

void Gamestate_UpdateLogo(float dt) {
    (void)dt;
    if (Input_IsKeyHit(VK_ESCAPE)) {
        Game_ChangeState(STATE_LOGO_ENTER);
        return;
    }
    switch (g_game.state) {
    case STATE_LOGO_ENTER:
        if (padHit(0, PAD_C) || padHit(1, PAD_C)) {
            Audio_Play(g_waveSoundIds[SND_3_2], false);
            Game_ChangeState(STATE_INTRO);
            return;
        }
        if (g_game.bgaFrame >= g_game.bgaMaxFrame && g_game.stateFrame > 60)
            Game_ChangeState(STATE_INTRO); /* Exceed: 81 -> INTRO.MOV */
        break;
    case STATE_LOGO_SKIP:
        Game_ChangeState(STATE_MENU_ENTER);
        break;
    default:
        break;
    }
}
