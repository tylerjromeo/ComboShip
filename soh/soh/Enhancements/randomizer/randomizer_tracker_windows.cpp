#ifdef COMBO_BUILD
#include <ship/window/gui/SecondaryScreen.h>
#endif
#include "randomizer_tracker_windows.h"

#include <cstring>

#include <imgui_internal.h>
#include <ship/window/Window.h>

#include "soh/SohGui/UIWidgets.hpp"

namespace Trackers {
bool IsDockedToMainDeck(const std::string& UniqueName) {
    ImGuiWindow* window = ImGui::FindWindowByName(UniqueName.c_str());
    if (window == NULL) {
        return false;
    }
    ImGuiDockNode* node = window->DockNode;
    if (node == NULL && window->DockId != 0) {
        node = ImGui::DockBuilderGetNode(window->DockId);
    }
    if (node == NULL) {
        return false;
    }
    ImGuiWindow* host = ImGui::DockNodeGetRootNode(node)->HostWindow;
    return host != NULL && strncmp(host->Name, "Main - Deck", strlen("Main - Deck")) == 0;
}

bool BeginFloatWindows(const std::string& UniqueName, bool* open, const Color_RGBA8& bgCol,
                       TrackerWindowType windowType, bool draggable, ImGuiWindowFlags flags) {
    ImGuiWindowFlags windowFlags = flags;

#ifdef COMBO_BUILD
    // ComboShip (dual-screen Android): list trackers dock as tabs on the second display; the others are
    // moved there once. Docked windows can't be floating overlays.
    // Docked tabs sit next to MM's and ComboShip's, so they say which game they're for. "Label###ID"
    // changes only the visible text; the docked window keeps one stable ImGui identity.
    const bool listTracker =
        UniqueName == "Check Tracker" || UniqueName == "Entrance Tracker" || UniqueName == "Hint Tracker";
    const std::string dockedName = "OoT " + UniqueName + "###" + UniqueName;
    const bool dockedOnSecondScreen = listTracker && Ship::SecondaryScreen::DockOnSecondary(dockedName.c_str());
    if (dockedOnSecondScreen) {
        windowType = TRACKER_WINDOW_WINDOW;
    }
#else
    const bool dockedOnSecondScreen = false;
#endif

    if (windowFlags == 0) {
        windowFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoFocusOnAppearing;
    }

    if (windowType == TRACKER_WINDOW_FLOATING) {
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar |
                       ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;

        if (!draggable) {
            windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
        }
    }
    ImVec4 bgColVec = VecFromRGBA8(bgCol);
    if (IsDockedToMainDeck(UniqueName) || dockedOnSecondScreen) {
        bgColVec.w = 1.0f;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColVec);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
#ifdef COMBO_BUILD
    if (listTracker && !dockedOnSecondScreen) {
        Ship::SecondaryScreen::PlaceOnSecondary(UniqueName.c_str()); // ComboShip: list trackers go below
    } else if (!listTracker) {
        Ship::SecondaryScreen::PlaceOnPrimary(UniqueName.c_str()); // item trackers stay over the game
    }
#endif
#ifdef COMBO_BUILD
    if (dockedOnSecondScreen) {
        return ImGui::Begin(dockedName.c_str(), open, windowFlags);
    }
#endif
    return ImGui::Begin(UniqueName.c_str(), open, windowFlags);
}

void EndFloatWindows() {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::End();
}
} // namespace Trackers
