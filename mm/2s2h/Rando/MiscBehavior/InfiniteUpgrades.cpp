#include "MiscBehavior.h"

extern "C" {
#include "variables.h"
#include "functions.h"
}

// Infinite Upgrades (RO_INFINITE_UPGRADES): the last tier of a progressive chain sets a RANDO_INF_HAS_INFINITE_*
// flag (GiveItem.cpp). While the flag is set, keep that ammo full. Capacity never changes, so the HUD shows
// the usual green "full" count. Only bow and bomb bag (bombchus included): MM magic and rupees are left
// to Chateau Romani and the Tycoon's Wallet. Minigames use interfaceCtx.minigameAmmo, not AMMO(), so they're
// unaffected.
static void RefillInfiniteUpgrades() {
    if (gPlayState == NULL) {
        return;
    }

    if (Flags_GetRandoInf(RANDO_INF_HAS_INFINITE_QUIVER) && INV_CONTENT(ITEM_BOW) == ITEM_BOW) {
        AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
    }

    if (Flags_GetRandoInf(RANDO_INF_HAS_INFINITE_BOMB_BAG)) {
        // MM's bombchus share the bomb bag's capacity, so the infinite bag covers both.
        if (INV_CONTENT(ITEM_BOMB) == ITEM_BOMB) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        }
        if (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) {
            AMMO(ITEM_BOMBCHU) = CUR_CAPACITY(UPG_BOMB_BAG);
        }
    }
}

void Rando::MiscBehavior::InitInfiniteUpgrades() {
    bool shouldRegister = IS_RANDO && RANDO_SAVE_OPTIONS[RO_INFINITE_UPGRADES] != RO_INF_UPGRADES_OFF;

    COND_HOOK(OnGameStateUpdate, shouldRegister, RefillInfiniteUpgrades);
}
