// combo/gui/ComboCheckCountWindow.h
//
// ComboShip-owned check count overlay: one small line in the game display's upper-left corner
// showing checks found or skipped across BOTH games over the total shuffled checks in both games.
//
// Each half comes from that game's own tracker numbers through SOH_/MM_GetCheckCounts, so the
// overlay always agrees with the two check trackers' headers. The dormant game's numbers are its
// live in-memory ones; before MM's first visit on a slot, the slot's MM save is loaded for reading
// (same path as the dormant tracker draws).
#pragma once

#include <ship/window/gui/GuiWindow.h>

namespace ComboRando {

// Registered in ComboUI_Register alongside the other combo-owned windows.
void RegisterCheckCountWindow();

// Rows for the Settings > Check Tracker panel (toggle, per-game breakdown, scale). Returns true
// when a CVar changed so the caller can schedule a config save.
bool DrawCheckCountSettings();

class ComboCheckCountWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void Draw() override;

  protected:
    void InitElement() override {
    }
    void UpdateElement() override {
    }
    void DrawElement() override;
};

} // namespace ComboRando
