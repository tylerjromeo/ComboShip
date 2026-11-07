// combo/ComboDualScreen.h
// Where ComboShip's ImGui windows go on a dual-screen Android handheld (AYN Thor).
//
// libultraship's SecondaryScreen widens the ImGui canvas so the second display sits to the right
// of the game display; any window positioned inside that region is drawn on, and takes touches
// from, the second display (see libultraship/include/ship/window/gui/SecondaryScreen.h). So the
// main viewport spans BOTH displays there, and code that centers on it lands between them.
//
// The rule, after linkzenic's SoH Android port (soh/soh/SohGui/DualScreen.cpp there):
//   - menus, setup screens and modals -> MenuRect: the second display when present;
//   - in-game overlays (notifications, input viewer) -> GameRect: the game display only.
// Without a second display (desktop, ordinary phones) both are exactly the viewport work area
// the callers used before, so nothing changes there.
//
// Header-only: included by comboui, soh and 2ship alike; each calls straight into libultraship.
#pragma once

#include <imgui.h>
#include <ship/window/gui/SecondaryScreen.h>

#include <algorithm>

namespace ComboDualScreen {

inline bool IsActive() {
    return Ship::SecondaryScreen::IsActive();
}

// The game display's part of the main viewport's work area.
inline void GameRect(ImVec2& pos, ImVec2& size) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    pos = vp->WorkPos;
    size = vp->WorkSize;
    if (IsActive()) {
        const ImVec2 primary = Ship::SecondaryScreen::GetPrimarySize();
        size.x = std::min(size.x, primary.x - (vp->WorkPos.x - vp->Pos.x));
        size.y = std::min(size.y, primary.y - (vp->WorkPos.y - vp->Pos.y));
    }
}

// Where menus and modal screens belong: the second display when present, else the game display.
inline void MenuRect(ImVec2& pos, ImVec2& size) {
    if (IsActive()) {
        // Below the clock/battery strip along the second display's top edge.
        pos = Ship::SecondaryScreen::GetSecondaryWorkPos();
        size = Ship::SecondaryScreen::GetSecondaryWorkSize();
        return;
    }
    GameRect(pos, size);
}

inline ImVec2 Center(const ImVec2& pos, const ImVec2& size) {
    return ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
}

inline ImVec2 MenuCenter() {
    ImVec2 pos, size;
    MenuRect(pos, size);
    return Center(pos, size);
}

inline ImVec2 GameCenter() {
    ImVec2 pos, size;
    GameRect(pos, size);
    return Center(pos, size);
}

} // namespace ComboDualScreen
