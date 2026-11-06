#pragma once

// SecondaryScreen: lets ImGui windows (menus, trackers, ...) live on a second physical display, such as the bottom
// screen of a dual-screen Android handheld (AYN Thor, Surface Duo, ...).
//
// How it works: the ImGui canvas is widened so the secondary display sits to the right of the primary one:
//
//   x = 0                     x = PrimarySize.x            x = PrimarySize.x + SecondarySize.x
//   +-------------------------+----------------------------+
//   |  primary display        |  secondary display         |
//   |  (game + overlays)      |  (menus / trackers)        |
//   +-------------------------+----------------------------+
//
// Each frame the same ImDrawData is rendered twice: once onto the primary window (clipped to the left part) and once
// onto the secondary display's surface (offset to the right part). Touches on the secondary display are fed to ImGui
// at the matching canvas position. Any ImGui window whose position is inside the secondary rect therefore shows up,
// and is interactive, on the secondary display without any per-window code.
//
// When no secondary display is attached (or the feature is disabled) everything here is a no-op and the canvas is
// just the primary display, so single-screen devices behave exactly as before.

#include <imgui.h>

namespace Ship {
namespace SecondaryScreen {

// CVars (all optional).
#define CVAR_SECONDARY_SCREEN_ENABLED "gSettings.SecondaryScreen.Enabled" // default 1
#define CVAR_SECONDARY_SCREEN_SCALE "gSettings.SecondaryScreen.Scale"     // UI scale multiplier, default 1.0

// True when a secondary display is attached, its surface is ready and the feature is enabled.
bool IsActive();

// Size of the primary display in ImGui canvas units. Always valid (equals the whole canvas when inactive).
ImVec2 GetPrimarySize();
ImVec2 GetPrimaryCenter();

// Rect of the secondary display in ImGui canvas units. Only meaningful while IsActive().
ImVec2 GetSecondaryPos();
ImVec2 GetSecondarySize();
ImVec2 GetSecondaryCenter();

// True if the given canvas point lies on the secondary display (always false while inactive).
bool ContainsPoint(ImVec2 point);

// Helpers for code that wants to "center something on the screen the UI lives on".
// Returns the secondary rect when active, else the primary rect.
ImVec2 GetUiScreenPos();
ImVec2 GetUiScreenSize();
ImVec2 GetUiScreenCenter();

#ifdef COMBO_BUILD
// ComboShip: call right before ImGui::Begin(windowName, ...) of a tracker-style window. The first time
// it is seen while a secondary display is active, the window is moved onto that display (shelf-packed
// next to the others) unless it is already there or docked. After that it is left where the user
// drags it, and imgui.ini keeps it there. No-op without a secondary display.
void PlaceOnSecondary(const char* windowName);

// ComboShip: for list-style trackers (check, entrance, hint, notes). Call right before
// ImGui::Begin(windowName, ...): while a secondary display is active (and
// CVAR_SECONDARY_SCREEN_TRACKER_DOCK is on) the window is docked as a tab into a fixed full-screen
// dockspace on that display, and true is returned so the caller drops flags that forbid docking
// (NoDocking, a floating-overlay look). Returns false otherwise.
#define CVAR_SECONDARY_SCREEN_TRACKER_DOCK "gSettings.SecondaryScreen.TrackerDock" // default 1
bool DockOnSecondary(const char* windowName);
#endif

// --- Integration points (called by the GUI backend, not by game code) ---

// Call right after the platform backend's NewFrame (which sets io.DisplaySize to the primary window size) and before
// ImGui::NewFrame(). Widens io.DisplaySize and injects queued secondary-display touch events.
void NewFrame();

// Call right after ImGui::NewFrame(). Turns vertical finger drags on the secondary display into scrolling of the
// window under the finger (ImGui has no native touch scrolling). Horizontal drags (sliders) are left alone.
void AfterNewFrame();

// Renders `drawData` onto both displays. `renderFn` is the renderer backend's RenderDrawData function. Must be called
// on the render thread with the primary GL context current. Returns false if the secondary display is not active, in
// which case the caller should render `drawData` normally.
bool RenderDrawData(ImDrawData* drawData, void (*renderFn)(ImDrawData*));

} // namespace SecondaryScreen
} // namespace Ship
