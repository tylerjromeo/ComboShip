#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/ShipUtils.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#ifdef COMBO_BUILD
#include "rando/CrossForeign.h"              // ComboShip: cross-game gossip-stone pool
#include "Rando/MiscBehavior/MiscBehavior.h" // ComboShip: ComboRandoGen cache invalidation
#endif

#include <algorithm>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"

#include "overlays/actors/ovl_En_Gs/z_en_gs.h"
}

#define FIRST_GS_MESSAGE 0x20D1
#define SECOND_GS_MESSAGE 0x20C0
#define MOON_GS_MASK_MESSAGE 0x20D4

std::vector<std::string> flavorText = {
    "Good luck on your journey ...",
    "I hope you find what you're looking for ...",
    "... Evil is afoot",
    "Beware the moon's gaze",
    " .. It's dangerous to go alone",
};

// clang-format off
std::unordered_map<RandoItemId, u32> riToWeight = {
    { RI_SOUL_BOSS_MAJORA, 13 },
    { RI_MASK_DEKU, 12 },
    { RI_MASK_GORON, 12 },
    { RI_MASK_ZORA, 12 },
    { RI_MASK_BLAST, 11 },
    { RI_MASK_FIERCE_DEITY, 11 },
    { RI_SOUL_BOSS_GOHT, 10 },
    { RI_SOUL_BOSS_GYORG, 10 },
    { RI_SOUL_BOSS_ODOLWA, 10 },
    { RI_SOUL_BOSS_TWINMOLD, 10 },
    { RI_REMAINS_GOHT, 10 },
    { RI_REMAINS_GYORG, 10 },
    { RI_REMAINS_ODOLWA, 10 },
    { RI_REMAINS_TWINMOLD, 10 },
};

std::unordered_map<RandoCheckId, u32> rcToWeight = {
    { RC_PINNACLE_ROCK_REUNITE_SEAHORSE, 10 },
    { RC_GREAT_BAY_COAST_NEW_WAVE_BOSSA_NOVA, 10 },
    { RC_MOUNTAIN_VILLAGE_FROG_CHOIR, 10 },
    { RC_STOCK_POT_INN_COUPLES_MASK, 10 },
    { RC_ROMANI_RANCH_ALIENS, 10 },
    { RC_WATERFALL_RAPIDS_BEAVER_RACE_01, 8 },
    { RC_WATERFALL_RAPIDS_BEAVER_RACE_02, 8 },
    { RC_KEATON_QUIZ, 8 },
    { RC_CURIOSITY_SHOP_SPECIAL_ITEM, 8 },
    { RC_DEKU_PLAYGROUND_ALL_DAYS, 8 },
    { RC_MOON_TRIAL_ZORA_PIECE_OF_HEART, 6 },
    { RC_MOON_TRIAL_DEKU_PIECE_OF_HEART, 6 },
    { RC_MOON_TRIAL_GORON_PIECE_OF_HEART, 6 },
};

std::unordered_map<RandoItemType, u32> itemTypeToWeight = {
    { RITYPE_MAJOR, 9 },
    { RITYPE_MASK, 9 },
    { RITYPE_BOSS_KEY, 8 },
    { RITYPE_LESSER, 6 },
    { RITYPE_SMALL_KEY, 5 },
    { RITYPE_SKULLTULA_TOKEN, 3 },
    { RITYPE_STRAY_FAIRY, 3 },
    { RITYPE_HEALTH, 2 },
    { RITYPE_JUNK, 2 },
};
// clang-format on

s32 GetNormalizedCost() {
    s32 obtainedChecks = 0;
    s32 maxChecks = 0;
    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        if (saveCheck.shuffled) {
            maxChecks++;
            if (saveCheck.obtained) {
                obtainedChecks++;
            }
        }
    }

    return MAX(10, MIN(250, 10 + (obtainedChecks * (250 - 10)) / (maxChecks)));
}

EnGs* GetGossipStone() {
    Player* player = GET_PLAYER(gPlayState);
    if (player->talkActor == nullptr || player->talkActor->id != ACTOR_EN_GS) {
        return nullptr;
    }
    return (EnGs*)player->talkActor;
}

bool ShouldHintMoonMask(EnGs* enGs) {
    return RANDO_SAVE_OPTIONS[RO_HINTS_MOON_GOSSIP_STONES] && enGs != nullptr && enGs->actor.params == ENGS_0 &&
           enGs->unk_195 >= 1 && enGs->unk_195 <= 20; // 20 moon gossip stones
}

// outForeignText: ComboShip out-param — set when the pick lands on a cross-game (OOT) hint, which
// has no RandoCheckId. Caller must display it directly instead of resolving a check/item name.
// outForeignIndex: ComboShip (#164) report-only out-param — that hint's gossipPool index.
// enGsOverride: ComboShip — roll for this stone instead of the one being talked to (unread-hint markers).
RandoCheckId GetRandomCheck(bool repeatableOnlyObtained = false, std::string* outForeignText = nullptr,
                            int* outForeignIndex = nullptr, EnGs* enGsOverride = nullptr) {
    EnGs* enGs = enGsOverride != nullptr ? enGsOverride : GetGossipStone();
    if (enGs == nullptr) {
        return RC_UNKNOWN;
    }

    u32 strength = RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONE_STRENGTH];

    std::vector<std::pair<RandoCheckId, u32>> weightedChecks;
    u32 totalWeight = 0;

    for (auto& [randoCheckId, _] : Rando::StaticData::Checks) {
        RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];
        auto& item = Rando::StaticData::Items[saveCheck.randoItemId];
        if (!saveCheck.shuffled || item.randoItemType == RITYPE_JUNK ||
            (repeatableOnlyObtained && saveCheck.obtained)) {
            continue;
        }

        u32 baseWeight = 1;
        if (rcToWeight.contains(randoCheckId)) {
            baseWeight = rcToWeight[randoCheckId];
        } else if (riToWeight.contains(saveCheck.randoItemId)) {
            baseWeight = riToWeight[saveCheck.randoItemId];
        } else if (itemTypeToWeight.contains(item.randoItemType)) {
            baseWeight = itemTypeToWeight[item.randoItemType];
        }

        u32 effectiveWeight = 100 + (baseWeight - 1) * strength;
        totalWeight += effectiveWeight;
        weightedChecks.push_back({ randoCheckId, totalWeight });
    }

    // ComboShip: fold OOT-owned cross-game hints into the SAME weighted draw (never a second RNG
    // source). Excluded when repeatableOnlyObtained (purchasable repeat hint) — MM can't see OOT's
    // obtained-state, so a cross entry could repeat an already-collected OOT item.
    std::vector<std::string> comboTexts;
    std::vector<std::pair<size_t, u32>> comboWeighted;
#ifdef COMBO_BUILD
    if (!repeatableOnlyObtained) {
        int slot = gSaveContext.fileNum;
        static uint64_t s_hintsGen = (uint64_t)-1;
        static ComboRando::MmHints s_hints;
        if (s_hintsGen != Rando::MiscBehavior::ComboRandoGen()) {
            s_hints = ComboRando::LoadHintsMM(slot);
            s_hintsGen = Rando::MiscBehavior::ComboRandoGen();
        }
        for (auto& g : s_hints.gossipPool) {
            u32 w = std::max<uint32_t>(1, g.weight);
            totalWeight += 100 + (w - 1) * strength;
            comboTexts.push_back(g.text);
            comboWeighted.push_back({ comboTexts.size() - 1, totalWeight });
        }
    }
#endif

    if (weightedChecks.empty() && comboWeighted.empty()) {
        return RC_UNKNOWN;
    }

    if (repeatableOnlyObtained) {
        Ship_Random_Seed(gGameState->frames);
    } else {
        // Convert through a signed 64-bit int: a negative float -> uint32_t is UB, and arm64 (fcvtzu)
        // clamps it to 0, so every stone with a negative coordinate sum shared one hint. Going through
        // int64_t matches what x86 already did, so desktop seeds keep their hints.
        float posSum = gPlayState->sceneId + enGs->actor.home.pos.x + enGs->actor.home.pos.z;
        uint32_t seed = (uint32_t)(int64_t)posSum;
        // Generic grottos all reuse one room of SCENE_KAKUSIANA with the stone at the same spot; mix
        // in the grotto ID (the entrance's chest/contents param) so each grotto gets its own hint.
        if (gPlayState->sceneId == SCENE_KAKUSIANA) {
            seed += (uint32_t)gSaveContext.respawn[RESPAWN_MODE_UNK_3].data * 0x9E3779B1u;
        }
        Ship_Random_Seed(gSaveContext.save.shipSaveInfo.rando.finalSeed + seed);
    }

    u32 roll = Ship_Random(0, totalWeight);
    for (auto& [checkId, cumWeight] : weightedChecks) {
        if (roll < cumWeight) {
            return checkId;
        }
    }
    for (auto& [comboIdx, cumWeight] : comboWeighted) {
        if (roll < cumWeight) {
            if (outForeignText) {
                *outForeignText = comboTexts[comboIdx];
            }
            if (outForeignIndex) {
                *outForeignIndex = (int)comboIdx;
            }
            return RC_UNKNOWN;
        }
    }
    return weightedChecks.empty() ? RC_UNKNOWN : weightedChecks.back().first;
}

#ifdef COMBO_BUILD
// ComboShip (#164): combo Hint Tracker reveal sink (the launcher registers it, see BenPort.cpp).
extern "C" void (*gMMComboHintReveal)(int fileNum, int kind, int poolIndex, const char* key, const char* text);

// Report a stone hint the game is about to display: a cross-game pool pick by index (kind 0), or a
// native MM check by its combo-spoiler name (kind 1, plain text — no color codes). A trap check's
// disguise name re-rolls per scene init, so the launcher dedupes kind 1 on the check name alone.
// The launcher's read-state key for a native MM stone hint (kind 1): the check's combo-spoiler name.
static std::string ComboStoneCheckKey(RandoCheckId rc) {
    const std::string& display = Rando::StaticData::GetCheckDisplayName(rc);
    return display.empty() ? Rando::StaticData::CheckNames[rc] : display;
}

static void ComboReportStoneHint(int poolIndex, RandoCheckId rc, bool showExact) {
    if (gMMComboHintReveal == nullptr || gSaveContext.fileNum == 0xFF) {
        return;
    }
    if (poolIndex >= 0) {
        gMMComboHintReveal(gSaveContext.fileNum, 0, poolIndex, "", "");
        return;
    }
    if (rc == RC_UNKNOWN) {
        return;
    }
    const std::string key = ComboStoneCheckKey(rc);
    const std::string text = Rando::StaticData::GetItemName(RANDO_SAVE_CHECKS[rc].randoItemId, true, rc) +
                             " is hidden " + Rando::StaticData::GetLocationNameForHint(rc, showExact);
    gMMComboHintReveal(gSaveContext.fileNum, 1, -1, key.c_str(), text.c_str());
}
// ComboShip: unread-hint markers. A slow trickle of sparkles over every gossip stone whose hint the
// player hasn't read yet, so it's obvious which stones are still worth talking to. Read state lives in
// the launcher's per-slot container (the same one the combo Hint Tracker shows).
extern "C" int (*gMMComboHintIsRead)(int fileNum, int kind, int poolIndex, const char* key);

namespace {

constexpr const char* kCvarUnreadMarkers = "gCombo.HintTracker.UnreadMarkers";
constexpr const char* kCvarUnreadMarkerColor = "gCombo.HintTracker.UnreadMarkerColor";
constexpr Color_RGBA8 kDefaultUnreadMarkerColor = { 120, 220, 255, 255 };
constexpr u32 kMarkerSpawnInterval = 5;
constexpr f32 kMarkerMaxDistance = 2500.0f;

// What a stone will say, in the launcher's read-state terms. kind -1 = nothing trackable.
struct StoneHintId {
    int kind = -1;
    int poolIndex = -1;
    std::string key;
    uint64_t gen = 0;
};

std::unordered_map<Actor*, StoneHintId> sStoneHintIds;

// Mirrors the text hooks below: a moon stone's mask hint wins, else the free stone hint (if enabled).
StoneHintId ComputeStoneHintId(EnGs* enGs) {
    StoneHintId id;
    id.gen = Rando::MiscBehavior::ComboRandoGen();
    if (ShouldHintMoonMask(enGs)) {
        RandoItemId randoItemId = Rando::StaticData::GetItemIdFromVanillaItemId(ITEM_MASK_TRUTH + enGs->unk_195 - 1);
        RandoCheckId randoCheckId = Rando::FindItemPlacement(randoItemId);
        // A mask placed in OOT is hinted through the NPC itemLocations text, which isn't always
        // reported; leave those unmarked rather than sparkle forever.
        if (randoCheckId != RC_UNKNOWN) {
            id.kind = 1;
            id.key = ComboStoneCheckKey(randoCheckId);
        }
        return id;
    }
    if (!RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONES]) {
        return id;
    }
    // GetRandomCheck reseeds the shared Ship_Random stream; put it back so looking ahead is invisible.
    u64 rngState;
    u8 rngSeeded;
    Ship_Random_GetState(&rngState, &rngSeeded);
    std::string foreignText;
    int foreignIndex = -1;
    RandoCheckId randoCheckId = GetRandomCheck(false, &foreignText, &foreignIndex, enGs);
    Ship_Random_SetState(rngState, rngSeeded);
    if (!foreignText.empty()) {
        if (foreignIndex >= 0) {
            id.kind = 0;
            id.poolIndex = foreignIndex;
        }
    } else if (randoCheckId != RC_UNKNOWN) {
        id.kind = 1;
        id.key = ComboStoneCheckKey(randoCheckId);
    }
    return id;
}

// Tiny local LCG for sparkle jitter, so markers never draw from the game's RNG.
u32 sMarkerJitter = 0x2468ACEu;
f32 MarkerJitter(f32 range) {
    sMarkerJitter = sMarkerJitter * 1664525u + 1013904223u;
    return ((f32)(sMarkerJitter >> 8) / (f32)(1u << 24) * 2.0f - 1.0f) * range;
}

void UpdateUnreadStoneMarker(EnGs* enGs) {
    if (gMMComboHintIsRead == nullptr || gSaveContext.fileNum == 0xFF || !CVarGetInteger(kCvarUnreadMarkers, 1)) {
        return;
    }
    if ((gPlayState->gameplayFrames % kMarkerSpawnInterval) != 0) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || Math_Vec3f_DistXYZ(&enGs->actor.world.pos, &player->actor.world.pos) > kMarkerMaxDistance) {
        return;
    }
    auto it = sStoneHintIds.find(&enGs->actor);
    if (it == sStoneHintIds.end() || it->second.gen != Rando::MiscBehavior::ComboRandoGen()) {
        it = sStoneHintIds.insert_or_assign(&enGs->actor, ComputeStoneHintId(enGs)).first;
    }
    const StoneHintId& id = it->second;
    if (id.kind < 0 || gMMComboHintIsRead(gSaveContext.fileNum, id.kind, id.poolIndex, id.key.c_str()) != 0) {
        return;
    }

    Color_RGBA8 color = CVarGetColor(kCvarUnreadMarkerColor, kDefaultUnreadMarkerColor);
    Color_RGBA8 prim = { (u8)MIN(255, color.r + 100), (u8)MIN(255, color.g + 100), (u8)MIN(255, color.b + 100),
                         255 };
    Color_RGBA8 env = { color.r, color.g, color.b, 0 };
    Vec3f pos = { enGs->actor.world.pos.x + MarkerJitter(12.0f), enGs->actor.world.pos.y + 58.0f + MarkerJitter(6.0f),
                  enGs->actor.world.pos.z + MarkerJitter(12.0f) };
    Vec3f velocity = { 0.0f, 0.6f, 0.0f };
    Vec3f accel = { 0.0f, 0.0f, 0.0f };
    EffectSsKirakira_SpawnDispersed(gPlayState, &pos, &velocity, &accel, &prim, &env, 1400, 24);
}

} // namespace
#else
static void ComboReportStoneHint(int, RandoCheckId, bool) {
}
#endif

// ComboShip: the mask may sit in OOT; resolve it combo-aware and report to the Hint Tracker. Kept
// out of the hook macro: an #ifdef inside a macro argument doesn't compile on MSVC.
static std::string MoonMaskLocationHint(RandoItemId randoItemId, RandoCheckId randoCheckId) {
#ifdef COMBO_BUILD
    ComboReportStoneHint(-1, randoCheckId, false);
    return Rando::GetItemLocationHintName(randoItemId, false);
#else
    return Rando::StaticData::GetLocationNameForHint(randoCheckId, false);
#endif
}

void Rando::ActorBehavior::InitEnGsBehavior() {
    bool randomHints =
        IS_RANDO && (RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONES] || RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]);
    bool moonMaskHints = IS_RANDO && RANDO_SAVE_OPTIONS[RO_HINTS_MOON_GOSSIP_STONES];

#ifdef COMBO_BUILD
    // ComboShip: unread-hint markers (see UpdateUnreadStoneMarker).
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_GS, randomHints || moonMaskHints,
                 [](Actor* actor) { UpdateUnreadStoneMarker((EnGs*)actor); });
    COND_ID_HOOK(OnActorDestroy, ACTOR_EN_GS, randomHints || moonMaskHints,
                 [](Actor* actor) { sStoneHintIds.erase(actor); });
#endif

    COND_VB_SHOULD(VB_GS_CONSIDER_MASK_OF_TRUTH_EQUIPPED, randomHints, { *should = true; });

    COND_VB_SHOULD(VB_GS_CONSIDER_MASK_OF_TRUTH_EQUIPPED, moonMaskHints, {
        if (ShouldHintMoonMask(va_arg(args, EnGs*))) {
            *should = true;
        }
    });

    // Override the message ID so that we can control the text
    COND_VB_SHOULD(VB_GS_CONTINUE_TEXTBOX, randomHints, {
        // Moon stones have a mask hint of their own, which takes priority
        if (ShouldHintMoonMask(va_arg(args, EnGs*))) {
            return;
        }

        *should = false;
        Message_ContinueTextbox(gPlayState, SECOND_GS_MESSAGE);
    });

    COND_VB_SHOULD(VB_GS_CONTINUE_TEXTBOX, moonMaskHints, {
        if (!ShouldHintMoonMask(va_arg(args, EnGs*))) {
            return;
        }

        *should = false;
        Message_ContinueTextbox(gPlayState, MOON_GS_MASK_MESSAGE);
    });

    COND_ID_HOOK(OnOpenText, FIRST_GS_MESSAGE, randomHints, [](u16* textId, bool* loadFromMessageTable) {
        // Moon stones keep their vanilla opening line and answer with a mask hint instead
        if (ShouldHintMoonMask(GetGossipStone())) {
            return;
        }

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

        if (RANDO_SAVE_OPTIONS[RO_HINTS_GOSSIP_STONES]) {
            std::string foreignText; // ComboShip: set when the pick is a cross-game (OOT) hint
            int foreignIndex = -1;   // ComboShip (#164): its gossipPool index, for the Hint Tracker
            RandoCheckId randoCheckId = GetRandomCheck(false, &foreignText, &foreignIndex);
            if (randoCheckId == RC_UNKNOWN && foreignText.empty()) {
                return;
            }

            entry.autoFormat = false;

            if (!foreignText.empty()) {
                entry.msg = "They say " + foreignText + ".";
                ComboReportStoneHint(foreignIndex, RC_UNKNOWN, false);
            } else {
                auto& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];

                bool showExact = false;
                if (rcToWeight.contains(randoCheckId)) {
                    showExact = true;
                }

                entry.msg = "They say %g{{item}}%w is hidden %y{{location}}%w.";

                CustomMessage::Replace(&entry.msg, "{{item}}",
                                       Rando::StaticData::GetItemName(saveCheck.randoItemId, true, randoCheckId));
                CustomMessage::Replace(&entry.msg, "{{location}}",
                                       Rando::StaticData::GetLocationNameForHint(randoCheckId, showExact));
                ComboReportStoneHint(-1, randoCheckId, showExact);
            }

            // Replace colors before line break calculation
            CustomMessage::ReplaceColorChars(&entry.msg);

            CustomMessage::AddLineBreaks(&entry.msg);

            if (RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]) {
                entry.msg += "\x10...\x13\x12";
            }
        } else {
            entry.msg = "";
        }

        if (RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]) {
            entry.msg += "Trade %r{{rupees}} Rupees%w for a hint?\x02\x11\xC2No\x11Yes";
            s32 cost = GetNormalizedCost();
            CustomMessage::Replace(&entry.msg, "{{rupees}}", std::to_string(cost));

            CustomMessage::ReplaceColorChars(&entry.msg);
        }

        CustomMessage::EnsureMessageEnd(&entry.msg);

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    COND_ID_HOOK(OnOpenText, SECOND_GS_MESSAGE, randomHints, [](u16* textId, bool* loadFromMessageTable) {
        MessageContext* msgCtx = &gPlayState->msgCtx;
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

        if (RANDO_SAVE_OPTIONS[RO_HINTS_PURCHASEABLE]) {
            if (msgCtx->choiceIndex == 1) {
                s32 cost = GetNormalizedCost();

                RandoCheckId randoCheckId = GetRandomCheck(true);
                if (gSaveContext.save.saveInfo.playerData.rupees < cost) {
                    entry.msg = "Foolish... You don't have enough rupees...";
                } else if (randoCheckId == RC_UNKNOWN) {
                    entry.msg = "I have no more hints for you...";
                } else {
                    RandoSaveCheck saveCheck = RANDO_SAVE_CHECKS[randoCheckId];

                    entry.msg = "Wise choice... They say %g{{item}}%w is hidden %y{{location}}%w.";

                    CustomMessage::Replace(&entry.msg, "{{item}}",
                                           Rando::StaticData::GetItemName(saveCheck.randoItemId, true, randoCheckId));
                    CustomMessage::Replace(&entry.msg, "{{location}}",
                                           Rando::StaticData::GetLocationNameForHint(randoCheckId, true));

                    gSaveContext.rupeeAccumulator -= cost;
                    cost *= 2;
                    ComboReportStoneHint(-1, randoCheckId, true); // paid for, so it really displays
                }
            } else {
                entry.msg = "Foolish... Come back later when you have more sense.";
            }
        } else {
            entry.msg = flavorText[Ship_Random(0, flavorText.size())];
        }

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    COND_ID_HOOK(OnOpenText, MOON_GS_MASK_MESSAGE, moonMaskHints, [](u16* textId, bool* loadFromMessageTable) {
        EnGs* enGs = GetGossipStone();
        if (!ShouldHintMoonMask(enGs)) {
            return;
        }

        RandoItemId randoItemId = Rando::StaticData::GetItemIdFromVanillaItemId(ITEM_MASK_TRUTH + enGs->unk_195 - 1);
        RandoCheckId randoCheckId = Rando::FindItemPlacement(randoItemId);

        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        entry.msg = "They say %g{{item}}%w is hidden %y{{location}}%w...";

        CustomMessage::Replace(&entry.msg, "{{item}}", Rando::StaticData::GetItemName(randoItemId));
        CustomMessage::Replace(&entry.msg, "{{location}}", MoonMaskLocationHint(randoItemId, randoCheckId));

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });

    // Four Gossip Stone Grottos Heart Piece item grant behavior override
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* refActor = va_arg(args, Actor*);
        Player* player = GET_PLAYER(gPlayState);

        if (refActor->id != ACTOR_EN_GS || *item != GI_HEART_PIECE) {
            return;
        }

        *should = false;

        refActor->parent = &player->actor;
    });
}
