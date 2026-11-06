#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/ShipInit.hpp"
#include "SavingEnhancements.h"
#include <spdlog/spdlog.h>

extern "C" PlayState* gPlayState;

#define CVAR_NAME "gEnhancements.Saving.PauseSave"
#define CVAR CVarGetInteger(CVAR_NAME, false)

extern "C" const char* SavingEnhancements_CannotSaveReason();

void RegisterPauseSave() {
    COND_VB_SHOULD(VB_SAVE_ON_B_BUTTON_IN_PAUSE_MENU, CVAR, {
        Input* input = CONTROLLER1(&gPlayState->state);
        *should = SavingEnhancements_CanSave() && CHECK_BTN_ALL(input->press.button, BTN_B);
        // ComboShip: B silently closing the menu looks like the feature is off; log why a save was refused.
        if (!*should && CHECK_BTN_ALL(input->press.button, BTN_B)) {
            const char* why = SavingEnhancements_CannotSaveReason();
            SPDLOG_INFO("[ComboShip] PauseSave: save refused ({}) scene={}", why ? why : "unknown",
                        (int)gPlayState->sceneId);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterPauseSave, { CVAR_NAME });
