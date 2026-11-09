// combo/gui/ComboCheckCountWindow.cpp — see ComboCheckCountWindow.h
#include <ship/window/gui/SecondaryScreen.h>
#include "ComboCheckCountWindow.h"
#include "ComboTrackerCommon.h" // OotActiveSlot
#include "ComboTrackerSwap.h"   // EnsureMmSaveLoaded for MM's dormant half
#include "ComboWidgetStyle.h"
#include <imgui.h>
#include <libultraship/libultraship.h> // CVar bridge
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include "ComboResolve.h" // Combo_ResolveSym for the SOH_/MM_GetCheckCounts exports

namespace ComboRando {

namespace {

constexpr const char* kCvarEnabled = "gCombo.CheckCount.Enabled";
constexpr const char* kCvarPerGame = "gCombo.CheckCount.PerGame";
constexpr const char* kCvarScale = "gCombo.CheckCount.Scale";
constexpr const char* kCvarHideBg = "gCombo.CheckCount.HideBackground";
constexpr const char* kWindowName = "Check Count##Combo";

const ImVec4 kWhite(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 kGreen(0.52f, 1.0f, 0.23f, 1.0f);
const ImVec4 kGrey(0.78f, 0.78f, 0.78f, 1.0f);

using Fn_GetCheckCounts = int (*)(int*, int*);

struct Counts {
    bool oot = false;
    bool mm = false;
    int ootFound = 0, ootTotal = 0;
    int mmFound = 0, mmTotal = 0;
};

Counts sCounts;
int sRefreshFrames = 0; // MM walks its whole check table; a few refreshes a second is plenty

void RefreshCounts() {
    static Fn_GetCheckCounts sOot = nullptr;
    static Fn_GetCheckCounts sMm = nullptr;
    static bool sTried = false;
    if (!sTried) {
        sTried = true;
        sOot = (Fn_GetCheckCounts)Combo_ResolveSym("soh", "SOH_GetCheckCounts");
        sMm = (Fn_GetCheckCounts)Combo_ResolveSym("2ship", "MM_GetCheckCounts");
    }
    ComboTracker::EnsureMmSaveLoaded(); // MM never visited on this slot: read its save, not boot defaults
    Counts c;
    c.oot = sOot && sOot(&c.ootFound, &c.ootTotal) != 0;
    c.mm = sMm && sMm(&c.mmFound, &c.mmTotal) != 0;
    sCounts = c;
}

void CountText(char* out, size_t n, int found, int total) {
    snprintf(out, n, "%d / %d", found, total);
}

} // namespace

void ComboCheckCountWindow::DrawElement() {
    const Counts& c = sCounts;
    const int found = (c.oot ? c.ootFound : 0) + (c.mm ? c.mmFound : 0);
    const int total = (c.oot ? c.ootTotal : 0) + (c.mm ? c.mmTotal : 0);
    char buf[32];
    CountText(buf, sizeof(buf), found, total);
    ImGui::TextColored(total > 0 && found >= total ? kGreen : kWhite, "Checks  %s", buf);
    if (CVarGetInteger(kCvarPerGame, 0) != 0) {
        if (c.oot) {
            CountText(buf, sizeof(buf), c.ootFound, c.ootTotal);
            ImGui::TextColored(c.ootFound >= c.ootTotal ? kGreen : kGrey, "OOT     %s", buf);
        }
        if (c.mm) {
            CountText(buf, sizeof(buf), c.mmFound, c.mmTotal);
            ImGui::TextColored(c.mmFound >= c.mmTotal ? kGreen : kGrey, "MM      %s", buf);
        }
    }
}

void ComboCheckCountWindow::Draw() {
    auto ctx = Ship::Context::GetRawInstance();
    if (!ctx || !ctx->GetWindow() || !ctx->GetWindow()->GetGui()) {
        return;
    }
    // comboui's per-module GImGui must point at the shared context (same pattern as ComboMenu).
    ImGui::SetCurrentContext(ctx->GetWindow()->GetGui()->GetImGuiContext());

    if (!IsVisible()) {
        return;
    }
    if (ComboTracker::OotActiveSlot() < 0) {
        sRefreshFrames = 0; // title/file select: no slot loaded; recount right away on the next load
        SyncVisibilityConsoleVariable();
        return;
    }

    if (sRefreshFrames-- <= 0) {
        sRefreshFrames = 10;
        try {
            RefreshCounts();
        } catch (...) { // never let a game-DLL call unwind into the Gui draw loop
            sCounts = Counts{};
        }
    }
    if (!sCounts.oot && !sCounts.mm) {
        SyncVisibilityConsoleVariable(); // vanilla (non-rando) save: nothing to count
        return;
    }

    const float scale = std::clamp(CVarGetFloat(kCvarScale, 1.0f), 0.5f, 5.0f);
    const float alpha = CVarGetInteger(kCvarHideBg, 0) != 0 ? 0.0f : 0.5f;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    // Upper-left of the game display by default; draggable, and imgui.ini keeps where it was moved.
    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_FirstUseEver);
    Ship::SecondaryScreen::PlaceOnPrimary(kWindowName); // stays on the game display (dual-screen Android)
    if (ImGui::Begin(kWindowName, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking)) {
        ImGui::SetWindowFontScale(scale);
        DrawElement();
    }
    ImGui::End();
    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(2);
    SyncVisibilityConsoleVariable();
}

namespace {
std::shared_ptr<ComboCheckCountWindow> sWindow;
}

void RegisterCheckCountWindow() {
    auto ctx = Ship::Context::GetRawInstance();
    if (!ctx || !ctx->GetWindow() || !ctx->GetWindow()->GetGui()) {
        return;
    }
    if (!sWindow) {
        // Visible by default; the ctor reads the persisted CVar when there is one.
        sWindow = std::make_shared<ComboCheckCountWindow>(kCvarEnabled, true, kWindowName);
    }
    ctx->GetWindow()->GetGui()->AddGuiWindow(sWindow);
}

bool DrawCheckCountSettings() {
    const ImVec4 theme = ComboMenu_ThemeColor();
    bool changed = false;

    ImGui::SeparatorText("Check count (both games)");
    ComboMenu_PushCheckbox(theme);
    bool shown = CVarGetInteger(kCvarEnabled, 1) != 0;
    if (ImGui::Checkbox("Show check count", &shown)) {
        ComboTracker::SetTracker({ kCvarEnabled, kWindowName }, shown);
        changed = true;
    }
    bool perGame = CVarGetInteger(kCvarPerGame, 0) != 0;
    if (ImGui::Checkbox("Per-game breakdown##checkcount", &perGame)) {
        CVarSetInteger(kCvarPerGame, perGame ? 1 : 0);
        changed = true;
    }
    bool hideBg = CVarGetInteger(kCvarHideBg, 0) != 0;
    if (ImGui::Checkbox("Hide background##checkcount", &hideBg)) {
        CVarSetInteger(kCvarHideBg, hideBg ? 1 : 0);
        changed = true;
    }
    ComboMenu_PopCheckbox();

    float scale = CVarGetFloat(kCvarScale, 1.0f);
    ComboMenu_PushSlider(theme);
    if (ImGui::SliderFloat("Scale##checkcount", &scale, 0.5f, 5.0f, "%.1fx")) {
        CVarSetFloat(kCvarScale, std::clamp(scale, 0.5f, 5.0f));
        changed = true;
    }
    ComboMenu_PopSlider();
    ImGui::TextDisabled("Found or skipped checks over all shuffled checks, OOT + MM. Drag it to move it.");
    return changed;
}

} // namespace ComboRando
