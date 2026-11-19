/**
 * Unread hint markers: a slow trickle of sparkles over every gossip stone, sign and NPC whose hint
 * the player has not read yet. Read state comes from the Hint Tracker (OnRandoHintRevealed), so a
 * source stops sparkling the moment its text is shown.
 *
 * Each actor is mapped to the hint(s) it gives ahead of time, mirroring the lookups the message
 * builders in Messages/GossipStoneHints.cpp and Messages/StaticHints.cpp do at talk time. Hint
 * sources that are not actors (warp songs, Saria's ocarina call) and hints that never fire
 * OnRandoHintRevealed (the Skulltula house rewards) are not marked.
 */

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizer_hint_tracker.h"

extern "C" {
#include <z64.h>
#include <macros.h>
#include <functions.h>
#include <variables.h>
#include "src/overlays/actors/ovl_En_Ge1/z_en_ge1.h"
#include "src/overlays/actors/ovl_En_Go2/z_en_go2.h"
#include "src/overlays/actors/ovl_Fishing/z_fishing.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kCvarEnabled = "gCombo.HintTracker.UnreadMarkers";
constexpr const char* kCvarColor = "gCombo.HintTracker.UnreadMarkerColor";
constexpr Color_RGBA8 kDefaultColor = { 120, 220, 255, 255 };

// One sparkle per source every kSpawnInterval frames; sources farther than kMaxDistance are skipped.
constexpr uint32_t kSpawnInterval = 5;
constexpr float kMaxDistance = 2500.0f;
constexpr size_t kMaxHintsPerActor = 2;

struct ActorHints {
    RandomizerHint hints[kMaxHintsPerActor] = { RH_NONE, RH_NONE };
    size_t count = 0;
    void Add(RandomizerHint rh) {
        if (rh != RH_NONE && count < kMaxHintsPerActor) {
            hints[count++] = rh;
        }
    }
};

RandomizerHint GossipStoneHint(Actor* stone) {
    if (RAND_GET_OPTION(RSK_GOSSIP_STONE_HINTS).Is(RO_GOSSIP_STONES_NONE)) {
        return RH_NONE;
    }
    int16_t hintParams = stone->params & 0xFF;
    if (Rando::StaticData::stoneParamsToHint.contains(hintParams)) {
        return Rando::StaticData::stoneParamsToHint[hintParams];
    }
    // Grotto stones share one params value; the grotto's chest identifies which grotto this is.
    // Same lookup as BuildHintStoneMessage.
    if (hintParams == 0x18) {
        for (size_t i = 0; i < ACTORCAT_MAX; i++) {
            Actor* head = gPlayState->actorCtx.actorLists[i].head;
            if (head != nullptr && head->id == ACTOR_EN_BOX &&
                Rando::StaticData::grottoChestParamsToHint.contains(head->params)) {
                return Rando::StaticData::grottoChestParamsToHint[head->params];
            }
        }
    }
    return RH_NONE;
}

ActorHints HintsForActor(Actor* actor) {
    ActorHints out;
    const int16_t scene = gPlayState->sceneNum;
    switch (actor->id) {
        case ACTOR_EN_GS:
            out.Add(GossipStoneHint(actor));
            break;
        case ACTOR_EN_WONDER_TALK:
            // Invisible talk spots; the type lives in params bits 11-15 (EnWonderTalk_Init).
            switch ((actor->params >> 0xB) & 0x1F) {
                case 1: // Temple of Time altar
                    if (scene == SCENE_TEMPLE_OF_TIME) {
                        out.Add(LINK_IS_ADULT ? RH_ALTAR_ADULT : RH_ALTAR_CHILD);
                    }
                    break;
                case 2: // Dampé's diary
                    out.Add(RH_DAMPES_DIARY);
                    break;
            }
            break;
        case ACTOR_EN_WONDER_TALK2:
            // textId = 0x200 | ((params >> 6) & 0xFF) (EnWonderTalk2_Init).
            switch ((actor->params >> 6) & 0xFF) {
                case 0x07: // Happy Mask Shop sign (TEXT_MASK_SHOP_SIGN)
                    out.Add(RH_MASK_SHOP_HINT);
                    break;
                case 0x2E: // Zora's River frogs, underwater (TEXT_FROGS_UNDERWATER)
                    out.Add(RH_FROGS_HINT);
                    break;
            }
            break;
        case ACTOR_EN_KANBAN:
            // textId = params | 0x300; 0x031A is the Horseback Archery sign (TEXT_GF_HBA_SIGN).
            if (actor->params == 0x1A && scene == SCENE_GERUDOS_FORTRESS) {
                out.Add(RH_HBA_HINT);
            }
            break;
        case ACTOR_EN_GE1:
            if ((actor->params & 0xFF) == GE1_TYPE_HORSEBACK_ARCHERY) {
                out.Add(RH_HBA_HINT);
            }
            break;
        case ACTOR_EN_TAKARA_MAN:
            out.Add(RH_GREG_RUPEE);
            break;
        case ACTOR_FISHING:
            if (actor->params == EN_FISH_OWNER) {
                out.Add(RH_LOACH_HINT);
                out.Add(RH_FISHING_POLE);
            }
            break;
        case ACTOR_EN_SA:
            if (scene == SCENE_SACRED_FOREST_MEADOW) {
                out.Add(RH_SARIA_HINT);
            }
            break;
        case ACTOR_EN_MD:
            out.Add(RH_MIDO_HINT);
            break;
        case ACTOR_EN_GO2:
            if ((actor->params & 0x1F) == GORON_DMT_BIGGORON) {
                out.Add(RH_BIGGORON_HINT);
            }
            break;
        case ACTOR_EN_GB:
            out.Add(RH_BIG_POES_HINT);
            break;
        case ACTOR_EN_NIW_LADY:
            // The hint rides on the child cucco-quest lines only.
            if (LINK_IS_CHILD) {
                out.Add(RH_CHICKENS_HINT);
            }
            break;
        case ACTOR_EN_MA2:
        case ACTOR_EN_MA3:
            out.Add(RH_MALON_HINT);
            break;
        case ACTOR_EN_XC:
            // Sheik only hints while the hinted item still matters (see BuildSheikMessage).
            if (scene == SCENE_TEMPLE_OF_TIME) {
                if (!OTRGlobals::Instance->gRandoContext->GetItemLocation(RC_SONG_FROM_OCARINA_OF_TIME)
                         ->HasObtained()) {
                    out.Add(RH_OOT_HINT);
                }
            } else if (scene == SCENE_INSIDE_GANONS_CASTLE) {
                if (INV_CONTENT(ITEM_ARROW_LIGHT) != ITEM_ARROW_LIGHT) {
                    out.Add(RH_SHEIK_HINT);
                }
            }
            break;
        default:
            break;
    }
    return out;
}

bool HasUnreadHint(Actor* actor) {
    ActorHints hints = HintsForActor(actor);
    auto ctx = OTRGlobals::Instance->gRandoContext;
    for (size_t i = 0; i < hints.count; i++) {
        if (ctx->GetHint(hints.hints[i])->IsEnabled() && !HintTracker::IsHintRead(hints.hints[i])) {
            return true;
        }
    }
    return false;
}

// Small local LCG for the sparkle jitter so the markers don't draw from the game's own RNG.
uint32_t sJitterState = 0x1234567u;
float Jitter(float range) {
    sJitterState = sJitterState * 1664525u + 1013904223u;
    return ((float)(sJitterState >> 8) / (float)(1u << 24) * 2.0f - 1.0f) * range;
}

void SpawnMarker(Actor* actor, Color_RGBA8 color) {
    // Sparkle above the actor's head: its focus point when that sits above the base (NPCs), else a
    // fixed height (stones, signs and invisible talk spots).
    float top = actor->focus.pos.y > actor->world.pos.y + 10.0f ? actor->focus.pos.y : actor->world.pos.y + 40.0f;
    Vec3f pos = { actor->world.pos.x + Jitter(12.0f), top + 18.0f + Jitter(6.0f), actor->world.pos.z + Jitter(12.0f) };
    Vec3f velocity = { 0.0f, 0.6f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    Color_RGBA8 prim = { (uint8_t)std::min(255, color.r + 100), (uint8_t)std::min(255, color.g + 100),
                         (uint8_t)std::min(255, color.b + 100), 255 };
    Color_RGBA8 env = { color.r, color.g, color.b, 0 };
    EffectSsKiraKira_SpawnDispersed(gPlayState, &pos, &velocity, &accel, &prim, &env, 1400, 24);
}

void OnPlayerUpdate() {
    if (gPlayState == nullptr || !CVarGetInteger(kCvarEnabled, 1)) {
        return;
    }
    if ((gPlayState->gameplayFrames % kSpawnInterval) != 0) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    Color_RGBA8 color = CVarGetColor(kCvarColor, kDefaultColor);
    for (size_t i = 0; i < ACTORCAT_MAX; i++) {
        for (Actor* actor = gPlayState->actorCtx.actorLists[i].head; actor != nullptr; actor = actor->next) {
            if (Math_Vec3f_DistXYZ(&actor->world.pos, &player->actor.world.pos) > kMaxDistance) {
                continue;
            }
            if (HasUnreadHint(actor)) {
                SpawnMarker(actor, color);
            }
        }
    }
}

void RegisterUnreadHintMarkers() {
    COND_HOOK(OnPlayerUpdate, IS_RANDO, OnPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterUnreadHintMarkers, { "IS_RANDO" });

} // namespace
