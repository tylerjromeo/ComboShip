#include "ship/window/gui/SecondaryScreen.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_set>

#include <imgui_internal.h>

#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"

namespace Ship {
namespace SecondaryScreen {

namespace {
// Per-frame layout, written in NewFrame() on the render thread and read by game/UI code on the same thread.
bool sFrameActive = false;
ImVec2 sPrimarySize = ImVec2(0.0f, 0.0f);
ImVec2 sSecondaryPos = ImVec2(0.0f, 0.0f);
ImVec2 sSecondarySize = ImVec2(0.0f, 0.0f);
float sSecondaryScale = 1.0f; // physical pixels per canvas unit on the secondary display

bool FeatureEnabled() {
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return true;
    }
    return ctx->GetConsoleVariables()->GetInteger(CVAR_SECONDARY_SCREEN_ENABLED, 1) != 0;
}

float UserScale() {
    auto ctx = Ship::Context::GetRawInstance();
    float scale = 1.0f;
    if (ctx != nullptr && ctx->GetConsoleVariables() != nullptr) {
        scale = ctx->GetConsoleVariables()->GetFloat(CVAR_SECONDARY_SCREEN_SCALE, 1.0f);
    }
    return std::clamp(scale, 0.25f, 4.0f);
}
} // namespace

bool IsActive() {
    return sFrameActive;
}

ImVec2 GetPrimarySize() {
    if (sPrimarySize.x <= 0.0f || sPrimarySize.y <= 0.0f) {
        return ImGui::GetIO().DisplaySize;
    }
    return sPrimarySize;
}

ImVec2 GetPrimaryCenter() {
    ImVec2 size = GetPrimarySize();
    return ImVec2(size.x * 0.5f, size.y * 0.5f);
}

ImVec2 GetSecondaryPos() {
    return sSecondaryPos;
}

ImVec2 GetSecondarySize() {
    return sSecondarySize;
}

ImVec2 GetSecondaryCenter() {
    return ImVec2(sSecondaryPos.x + sSecondarySize.x * 0.5f, sSecondaryPos.y + sSecondarySize.y * 0.5f);
}

namespace {
bool StatusBarEnabled() {
    auto ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return true;
    }
    return ctx->GetConsoleVariables()->GetInteger(CVAR_SECONDARY_SCREEN_STATUS_BAR, 1) != 0;
}

float StatusBarHeight() {
    if (!sFrameActive || !StatusBarEnabled() || GImGui == nullptr) {
        return 0.0f;
    }
    return std::floor(ImGui::GetFontSize() + 8.0f);
}
} // namespace

ImVec2 GetSecondaryWorkPos() {
    return ImVec2(sSecondaryPos.x, sSecondaryPos.y + StatusBarHeight());
}

ImVec2 GetSecondaryWorkSize() {
    return ImVec2(sSecondarySize.x, std::max(0.0f, sSecondarySize.y - StatusBarHeight()));
}

bool ContainsPoint(ImVec2 point) {
    return sFrameActive && point.x >= sSecondaryPos.x && point.y >= sSecondaryPos.y &&
           point.x < sSecondaryPos.x + sSecondarySize.x && point.y < sSecondaryPos.y + sSecondarySize.y;
}

ImVec2 GetUiScreenPos() {
    return sFrameActive ? sSecondaryPos : ImVec2(0.0f, 0.0f);
}

ImVec2 GetUiScreenSize() {
    return sFrameActive ? sSecondarySize : GetPrimarySize();
}

ImVec2 GetUiScreenCenter() {
    return sFrameActive ? GetSecondaryCenter() : GetPrimaryCenter();
}

#ifdef COMBO_BUILD
// ComboShip: see the header. State lives here, in the one shared libultraship, so trackers from
// soh, 2ship and comboui pack into the same shelves.
namespace {
std::unordered_set<std::string> sPlacedOnSecondary;
ImVec2 sShelfCursor = ImVec2(-1.0f, -1.0f);
float sShelfHeight = 0.0f;
} // namespace

void PlaceOnPrimary(const char* windowName) {
    if (!IsActive() || windowName == nullptr) {
        return;
    }
    ImGuiWindow* window = ImGui::FindWindowByName(windowName);
    if (window == nullptr || window->DockIsActive) {
        return; // not created yet (next frame then), or docked somewhere on purpose
    }
    // Every frame, not once: these windows auto-size to their contents and ComboShip lines MM's up
    // beside OoT's, so either can grow or be placed across the seam after any one-time move.
    const ImVec2 primary = GetPrimarySize();
    const float margin = 8.0f;
    if (window->Pos.x + window->Size.x > primary.x - margin) {
        const float x = std::max(margin, primary.x - window->Size.x - margin);
        ImGui::SetNextWindowPos(ImVec2(x, std::clamp(window->Pos.y, 0.0f, std::max(0.0f, primary.y - 32.0f))),
                                ImGuiCond_Always);
    }
}

namespace {
std::mutex sTabRequestMutex;
std::string sTabRequest;
int sTabRequestFrames = 0;

ImGuiID TrackerDockId();

// The tab picked by SelectSecondaryTab stays picked until the user taps the tab bar. ImGui itself
// moves the selection when tabs come and go (it selects newly appearing tabs, and restores the old
// selection when the tab bar is rebuilt during a game switch), which used to snap it back to some
// other tracker. Only a real tap on the tab bar counts as the user choosing.
std::string sStickyTab;
int sLastTabBarTapFrame = -1000;

// Render thread, inside DrawTrackerDockHost (after the DockSpace call, so this frame's tab bar
// update has already happened).
void ApplyTabRequest() {
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(TrackerDockId());
    ImGuiTabBar* tabBar = node != nullptr ? node->TabBar : nullptr;
    const int frame = ImGui::GetFrameCount();
    if (tabBar != nullptr && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        tabBar->BarRect.Contains(ImGui::GetIO().MousePos)) {
        sLastTabBarTapFrame = frame;
    }

    std::string request;
    {
        std::lock_guard<std::mutex> lock(sTabRequestMutex);
        request = sTabRequest;
        if (!request.empty()) {
            sStickyTab = request; // a new request replaces the old sticky tab
            sLastTabBarTapFrame = -1000;
        }
    }
    if (sStickyTab.empty()) {
        return;
    }

    ImGuiWindow* window = ImGui::FindWindowByName(sStickyTab.c_str());
    const bool docked = window != nullptr && window->DockIsActive && window->DockNode == node && tabBar != nullptr &&
                        ImGui::TabBarFindTabByID(tabBar, window->TabId) != nullptr;
    if (!request.empty()) {
        // Handed over to sStickyTab; a fresh request only needs to survive until its window shows up.
        if (docked || ++sTabRequestFrames >= 600) {
            std::lock_guard<std::mutex> lock(sTabRequestMutex);
            if (sTabRequest == request) {
                sTabRequest.clear();
            }
        }
    }
    if (!docked || tabBar->SelectedTabId == window->TabId) {
        return; // not shown yet (keeps waiting), or already the selected tab
    }

    // A tap on the tab bar in the last few frames: the user picked another tab. Stop sticking.
    if (frame - sLastTabBarTapFrame <= 30) {
        sStickyTab.clear();
        return;
    }
    tabBar->NextSelectedTabId = window->TabId;
    tabBar->SelectedTabId = window->TabId;
    node->SelectedTabId = window->TabId;
}

ImGuiID TrackerDockId() {
    return ImHashStr("SecondaryScreenTrackerDockSpace");
}

// A fixed, full-screen, invisible window on the secondary display hosting a dockspace. Docked windows
// show as tabs; the dockspace can't be moved, resized, split or undocked from, so finger drags only
// scroll. From linkzenic's SoH Android port (soh/soh/SohGui/DualScreen.cpp there).
void DrawTrackerDockHost() {
    static int sLastFrame = -1;
    const int frame = ImGui::GetFrameCount();
    if (sLastFrame == frame) {
        return;
    }
    sLastFrame = frame;
    // Callers may already have queued SetNextWindow* calls for their own window; keep them for it.
    ImGuiContext& g = *GImGui;
    const ImGuiNextWindowData callerNextWindow = g.NextWindowData;
    g.NextWindowData.ClearFlags();
    ImGui::SetNextWindowPos(GetSecondaryWorkPos(), ImGuiCond_Always);
    ImGui::SetNextWindowSize(GetSecondaryWorkSize(), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##SecondaryScreenTrackerDock", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(TrackerDockId(), ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_NoDockingSplit |
                         ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoWindowMenuButton |
                         ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
    ApplyTabRequest();
    g.NextWindowData = callerNextWindow;
}
} // namespace

bool DockOnSecondary(const char* windowName) {
    if (windowName == nullptr) {
        return false;
    }
    auto ctx = Ship::Context::GetRawInstance();
    const bool dockEnabled = ctx == nullptr || ctx->GetConsoleVariables() == nullptr ||
                             ctx->GetConsoleVariables()->GetInteger(CVAR_SECONDARY_SCREEN_TRACKER_DOCK, 1) != 0;
    const bool dock = IsActive() && dockEnabled;
    if (!dock) {
        ImGuiWindow* window = ImGui::FindWindowByName(windowName);
        if (window != nullptr && window->DockId == TrackerDockId()) {
            // Docking turned off or the secondary display went away: float it again on the game display.
            ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Always);
        }
        return false;
    }
    DrawTrackerDockHost();
    ImGui::SetNextWindowDockID(TrackerDockId(), ImGuiCond_Always);
    return true;
}

void SelectSecondaryTab(const char* windowName) {
    if (windowName == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(sTabRequestMutex);
    sTabRequest = windowName;
    sTabRequestFrames = 0;
}

void PlaceOnSecondary(const char* windowName) {
    if (!IsActive() || windowName == nullptr) {
        return;
    }
    if (!sPlacedOnSecondary.insert(windowName).second) {
        return; // placed once this session already; after that it stays wherever the user puts it
    }
    ImGuiWindow* window = ImGui::FindWindowByName(windowName);
    if (window != nullptr && (window->DockIsActive || ContainsPoint(window->Pos))) {
        return; // docked, or already on the second display (e.g. restored from imgui.ini)
    }
    const ImVec2 areaPos = GetSecondaryWorkPos();
    const ImVec2 areaSize = GetSecondaryWorkSize();
    const float margin = 8.0f;
    const ImVec2 size = (window != nullptr && window->Size.x > 1.0f) ? window->Size : ImVec2(320.0f, 320.0f);
    if (sShelfCursor.x < 0.0f) {
        sShelfCursor = ImVec2(areaPos.x + margin, areaPos.y + margin);
    }
    // Shelf packing: left to right, wrapping to a new row when the display's width is used up.
    if (sShelfCursor.x + size.x > areaPos.x + areaSize.x - margin && sShelfCursor.x > areaPos.x + margin) {
        sShelfCursor.x = areaPos.x + margin;
        sShelfCursor.y += sShelfHeight + margin;
        sShelfHeight = 0.0f;
    }
    // Past the bottom edge: wrap to the top again rather than off-screen (windows overlap instead).
    if (sShelfCursor.y + std::min(size.y, areaSize.y) > areaPos.y + areaSize.y) {
        sShelfCursor = ImVec2(areaPos.x + margin, areaPos.y + margin);
        sShelfHeight = 0.0f;
    }
    ImGui::SetNextWindowPos(sShelfCursor, ImGuiCond_Always);
    sShelfCursor.x += size.x + margin;
    sShelfHeight = std::max(sShelfHeight, size.y);
}
#endif

} // namespace SecondaryScreen
} // namespace Ship

#if defined(__ANDROID__)

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <SDL2/SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <vector>

#define SS_LOG(...) __android_log_print(ANDROID_LOG_INFO, "SoHSecondaryScreen", __VA_ARGS__)

namespace Ship {
namespace SecondaryScreen {
namespace {

struct TouchEvent {
    int action; // 0 = down, 1 = move, 2 = up/cancel
    float x;    // physical pixels on the secondary surface
    float y;
};

struct State {
    std::mutex mutex;
    std::condition_variable destroyedCv;

    // Written by JNI (Android UI thread), consumed on the render thread.
    ANativeWindow* pendingWindow = nullptr;
    bool pendingDestroy = false;
    float densityScale = 1.0f; // secondary density / primary density
    std::vector<TouchEvent> touches;

    // Render thread only.
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    ANativeWindow* window = nullptr;
    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;
    bool touchDown = false;
};

State sState;

// Requires sState.mutex. Render thread.
void DestroySurfaceLocked() {
    if (sState.display != EGL_NO_DISPLAY) {
        if (sState.surface != EGL_NO_SURFACE) {
            eglDestroySurface(sState.display, sState.surface);
        }
        if (sState.context != EGL_NO_CONTEXT) {
            eglDestroyContext(sState.display, sState.context);
        }
    }
    if (sState.window != nullptr) {
        ANativeWindow_release(sState.window);
    }
    sState.display = EGL_NO_DISPLAY;
    sState.context = EGL_NO_CONTEXT;
    sState.surface = EGL_NO_SURFACE;
    sState.window = nullptr;
    sState.surfaceWidth = 0;
    sState.surfaceHeight = 0;
}

// Requires sState.mutex. Render thread, primary GL context current. Takes ownership of `window` on success.
bool CreateSurfaceLocked(ANativeWindow* window) {
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext mainContext = eglGetCurrentContext();
    if (display == EGL_NO_DISPLAY || mainContext == EGL_NO_CONTEXT) {
        SS_LOG("No current EGL context; cannot create secondary surface");
        return false;
    }

    // Use the primary context's config so the shared context is guaranteed to be compatible.
    EGLint configId = 0;
    EGLint clientVersion = 3;
    eglQueryContext(display, mainContext, EGL_CONFIG_ID, &configId);
    eglQueryContext(display, mainContext, EGL_CONTEXT_CLIENT_VERSION, &clientVersion);

    EGLConfig config = nullptr;
    EGLint numConfigs = 0;
    const EGLint byIdAttribs[] = { EGL_CONFIG_ID, configId, EGL_NONE };
    if (eglChooseConfig(display, byIdAttribs, &config, 1, &numConfigs) != EGL_TRUE || numConfigs == 0) {
        const EGLint fallbackAttribs[] = { EGL_RENDERABLE_TYPE,
                                           EGL_OPENGL_ES3_BIT_KHR,
                                           EGL_SURFACE_TYPE,
                                           EGL_WINDOW_BIT,
                                           EGL_RED_SIZE,
                                           8,
                                           EGL_GREEN_SIZE,
                                           8,
                                           EGL_BLUE_SIZE,
                                           8,
                                           EGL_NONE };
        if (eglChooseConfig(display, fallbackAttribs, &config, 1, &numConfigs) != EGL_TRUE || numConfigs == 0) {
            SS_LOG("eglChooseConfig failed: 0x%x", eglGetError());
            return false;
        }
    }

    EGLint visualId = 0;
    if (eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visualId) == EGL_TRUE) {
        ANativeWindow_setBuffersGeometry(window, 0, 0, visualId);
    }

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, clientVersion, EGL_NONE };
    // Shared with the primary context, so ImGui's font texture, shader, buffers and the game framebuffer textures are
    // all usable here.
    EGLContext context = eglCreateContext(display, config, mainContext, ctxAttribs);
    if (context == EGL_NO_CONTEXT) {
        SS_LOG("eglCreateContext failed: 0x%x", eglGetError());
        return false;
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        SS_LOG("eglCreateWindowSurface failed: 0x%x", eglGetError());
        eglDestroyContext(display, context);
        return false;
    }

    // Don't let the secondary display's vsync throttle the game: set swap interval 0 on our surface.
    EGLDisplay prevDisplay = eglGetCurrentDisplay();
    EGLSurface prevDraw = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface prevRead = eglGetCurrentSurface(EGL_READ);
    EGLContext prevContext = eglGetCurrentContext();
    if (eglMakeCurrent(display, surface, surface, context) == EGL_TRUE) {
        eglSwapInterval(display, 0);
        eglMakeCurrent(prevDisplay, prevDraw, prevRead, prevContext);
    }

    sState.display = display;
    sState.context = context;
    sState.surface = surface;
    sState.window = window;
    SS_LOG("Secondary surface created (config %d, GLES %d)", configId, clientVersion);
    return true;
}

} // namespace

void NewFrame() {
    ImGuiIO& io = ImGui::GetIO();
    sPrimarySize = io.DisplaySize;

    std::vector<TouchEvent> touches;
    float densityScale = 1.0f;
    {
        std::lock_guard<std::mutex> lock(sState.mutex);

        if (sState.pendingDestroy) {
            DestroySurfaceLocked();
            sState.pendingDestroy = false;
            sState.destroyedCv.notify_all();
        }
        if (sState.pendingWindow != nullptr) {
            ANativeWindow* window = sState.pendingWindow;
            sState.pendingWindow = nullptr;
            DestroySurfaceLocked();
            if (!CreateSurfaceLocked(window)) {
                ANativeWindow_release(window);
            }
        }
        if (sState.surface != EGL_NO_SURFACE) {
            eglQuerySurface(sState.display, sState.surface, EGL_WIDTH, &sState.surfaceWidth);
            eglQuerySurface(sState.display, sState.surface, EGL_HEIGHT, &sState.surfaceHeight);
        }

        touches.swap(sState.touches);
        densityScale = sState.densityScale;

        sFrameActive = sState.surface != EGL_NO_SURFACE && sState.surfaceWidth > 0 && sState.surfaceHeight > 0 &&
                       FeatureEnabled();

        if (sFrameActive) {
            sSecondaryScale = std::max(0.1f, densityScale * UserScale());
            sSecondaryPos = ImVec2(sPrimarySize.x, 0.0f);
            sSecondarySize = ImVec2(std::floor(sState.surfaceWidth / sSecondaryScale),
                                    std::floor(sState.surfaceHeight / sSecondaryScale));
        }
    }

    if (!sFrameActive) {
        // Release a touch that was in progress when the display went away so ImGui doesn't think it's still held.
        if (sState.touchDown) {
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            sState.touchDown = false;
        }
        return;
    }

    io.DisplaySize = ImVec2(sPrimarySize.x + sSecondarySize.x, std::max(sPrimarySize.y, sSecondarySize.y));

    for (const TouchEvent& touch : touches) {
        ImVec2 pos(sSecondaryPos.x + touch.x / sSecondaryScale, sSecondaryPos.y + touch.y / sSecondaryScale);
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(pos.x, pos.y);
        if (touch.action == 0 && !sState.touchDown) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            sState.touchDown = true;
        } else if (touch.action == 2 && sState.touchDown) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            sState.touchDown = false;
        }
    }
}

namespace {
// Clock + battery strip along the top of the secondary display. Drawn on the foreground draw list so
// nothing covers it; the dock host and menus start below it (GetSecondaryWorkPos).
void DrawStatusBar() {
    const float height = StatusBarHeight();
    if (height <= 0.0f) {
        return;
    }

    // Battery: SDL asks Android's sticky ACTION_BATTERY_CHANGED intent over JNI, so poll it rarely.
    static double sNextBatteryPoll = 0.0;
    static int sBatteryPct = -1;
    static SDL_PowerState sPowerState = SDL_POWERSTATE_UNKNOWN;
    const double now = ImGui::GetTime();
    if (now >= sNextBatteryPoll) {
        sNextBatteryPoll = now + 20.0;
        int secs = -1;
        int pct = -1;
        sPowerState = SDL_GetPowerInfo(&secs, &pct);
        sBatteryPct = pct;
    }

    char clock[32] = "";
    const std::time_t t = std::time(nullptr);
    std::tm local{};
    if (localtime_r(&t, &local) != nullptr) {
        auto ctx = Ship::Context::GetRawInstance();
        const bool use24h = ctx != nullptr && ctx->GetConsoleVariables() != nullptr &&
                            ctx->GetConsoleVariables()->GetInteger(CVAR_SECONDARY_SCREEN_CLOCK_24H, 0) != 0;
        if (use24h) {
            std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
        } else {
            const int hour12 = local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12;
            std::snprintf(clock, sizeof(clock), "%d:%02d %s", hour12, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
        }
    }

    char battery[32] = "";
    if (sBatteryPct >= 0) {
        const bool charging = sPowerState == SDL_POWERSTATE_CHARGING || sPowerState == SDL_POWERSTATE_CHARGED;
        std::snprintf(battery, sizeof(battery), "%s%d%%", charging ? "+ " : "", sBatteryPct);
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const ImVec2 p0 = sSecondaryPos;
    const ImVec2 p1 = ImVec2(sSecondaryPos.x + sSecondarySize.x, sSecondaryPos.y + height);
    draw->AddRectFilled(p0, p1, IM_COL32(16, 16, 20, 255));
    draw->AddLine(ImVec2(p0.x, p1.y - 1.0f), ImVec2(p1.x, p1.y - 1.0f), IM_COL32(70, 70, 80, 255));

    const float pad = 10.0f;
    const float textY = p0.y + std::floor((height - ImGui::GetFontSize()) * 0.5f);
    const ImU32 textCol = IM_COL32(230, 230, 230, 255);
    draw->AddText(ImVec2(p0.x + pad, textY), textCol, clock);

    if (battery[0] != '\0') {
        const ImVec2 textSize = ImGui::CalcTextSize(battery);
        // A small battery glyph drawn with primitives (the default font has no battery icon).
        const float iconH = std::floor(ImGui::GetFontSize() * 0.6f);
        const float iconW = std::floor(iconH * 1.9f);
        const float iconX = p1.x - pad - iconW - 3.0f;
        const float iconY = p0.y + std::floor((height - iconH) * 0.5f);
        ImU32 fill = IM_COL32(110, 200, 110, 255);
        if (sBatteryPct <= 15) {
            fill = IM_COL32(230, 80, 70, 255);
        } else if (sBatteryPct <= 30) {
            fill = IM_COL32(230, 190, 70, 255);
        }
        draw->AddRect(ImVec2(iconX, iconY), ImVec2(iconX + iconW, iconY + iconH), textCol, 2.0f, 0, 1.5f);
        draw->AddRectFilled(ImVec2(iconX + iconW, iconY + iconH * 0.3f), ImVec2(iconX + iconW + 3.0f, iconY + iconH * 0.7f),
                            textCol);
        const float inner = (iconW - 4.0f) * std::clamp(sBatteryPct, 0, 100) / 100.0f;
        if (inner > 0.0f) {
            draw->AddRectFilled(ImVec2(iconX + 2.0f, iconY + 2.0f), ImVec2(iconX + 2.0f + inner, iconY + iconH - 2.0f),
                                fill, 1.0f);
        }
        draw->AddText(ImVec2(iconX - 6.0f - textSize.x, textY), textCol, battery);
    }
}
} // namespace

void AfterNewFrame() {
    if (sFrameActive) {
        DrawStatusBar();
    }

    static bool sTracking = false;
    static bool sScrolling = false;
    static ImGuiID sTargetId = 0;
    static ImVec2 sStart;

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    if (!sFrameActive || !io.MouseDown[0]) {
        sTracking = sScrolling = false;
        sTargetId = 0;
        return;
    }

    if (io.MouseClicked[0]) {
        // Only touches that land on the secondary display (mouse/primary-screen input is unaffected).
        sTracking = sState.touchDown && ContainsPoint(io.MousePos);
        sScrolling = false;
        sStart = io.MousePos;
        sTargetId = 0;
        if (sTracking) {
            // Scroll the innermost scrollable window under the finger (e.g. a table's scrolling region).
            for (ImGuiWindow* w = g.HoveredWindow; w != nullptr; w = w->ParentWindow) {
                if (w->ScrollMax.y > 0.0f && !(w->Flags & ImGuiWindowFlags_NoScrollWithMouse)) {
                    sTargetId = w->ID;
                    break;
                }
            }
        }
        return;
    }

    if (!sTracking || sTargetId == 0) {
        return;
    }
    ImGuiWindow* target = ImGui::FindWindowByID(sTargetId);
    if (target == nullptr) {
        sTracking = false;
        return;
    }

    const float dx = io.MousePos.x - sStart.x;
    const float dy = io.MousePos.y - sStart.y;
    if (!sScrolling && std::fabs(dy) > 12.0f && std::fabs(dy) > std::fabs(dx)) {
        sScrolling = true;
    }
    if (sScrolling) {
        // Cancel whatever the finger first pressed (button, tree node, window move) so lifting it isn't a click.
        ImGui::ClearActiveID();
        g.MovingWindow = nullptr;
        ImGui::SetScrollY(target, target->Scroll.y - io.MouseDelta.y);
    }
}

bool RenderDrawData(ImDrawData* drawData, void (*renderFn)(ImDrawData*)) {
    if (!sFrameActive || drawData == nullptr || renderFn == nullptr) {
        return false;
    }

    // Primary display: the left part of the canvas.
    ImDrawData primary = *drawData;
    primary.DisplayPos = ImVec2(0.0f, 0.0f);
    primary.DisplaySize = sPrimarySize;
    renderFn(&primary);

    std::lock_guard<std::mutex> lock(sState.mutex);
    if (sState.surface == EGL_NO_SURFACE) {
        return true;
    }

    EGLDisplay prevDisplay = eglGetCurrentDisplay();
    EGLSurface prevDraw = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface prevRead = eglGetCurrentSurface(EGL_READ);
    EGLContext prevContext = eglGetCurrentContext();

    if (eglMakeCurrent(sState.display, sState.surface, sState.surface, sState.context) != EGL_TRUE) {
        SS_LOG("eglMakeCurrent(secondary) failed: 0x%x", eglGetError());
        return true;
    }

    glViewport(0, 0, sState.surfaceWidth, sState.surfaceHeight);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Secondary display: the right part of the canvas, scaled up to the surface's physical resolution.
    ImDrawData secondary = *drawData;
    secondary.DisplayPos = sSecondaryPos;
    secondary.DisplaySize = sSecondarySize;
    secondary.FramebufferScale = ImVec2(sSecondaryScale, sSecondaryScale);
    renderFn(&secondary);

    eglSwapBuffers(sState.display, sState.surface);
    eglMakeCurrent(prevDisplay, prevDraw, prevRead, prevContext);
    return true;
}

} // namespace SecondaryScreen
} // namespace Ship

// --- JNI: called from com.dishii.soh.DualScreenManager on the Android UI thread ---

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_DualScreenManager_nativeSurfaceChanged(JNIEnv* env, jclass,
                                                                                          jobject surface, jint width,
                                                                                          jint height,
                                                                                          jfloat densityScale) {
    using namespace Ship::SecondaryScreen;
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(sState.mutex);
    if (sState.pendingWindow != nullptr) {
        ANativeWindow_release(sState.pendingWindow);
    }
    sState.pendingWindow = window;
    sState.pendingDestroy = false;
    sState.densityScale = densityScale > 0.0f ? densityScale : 1.0f;
    SS_LOG("Secondary surface changed: %dx%d, density scale %.2f", width, height, densityScale);
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_DualScreenManager_nativeSurfaceDestroyed(JNIEnv*, jclass) {
    using namespace Ship::SecondaryScreen;
    std::unique_lock<std::mutex> lock(sState.mutex);
    if (sState.pendingWindow != nullptr) {
        ANativeWindow_release(sState.pendingWindow);
        sState.pendingWindow = nullptr;
    }
    sState.touches.clear();
    if (sState.surface == EGL_NO_SURFACE) {
        return;
    }
    sState.pendingDestroy = true;
    // Android requires that we stop using the Surface before surfaceDestroyed() returns. Wait for the render thread to
    // release it; time out in case rendering is paused (the surface is then simply never touched again before the
    // render thread destroys it on its next frame).
    sState.destroyedCv.wait_for(lock, std::chrono::milliseconds(500), [] { return !sState.pendingDestroy; });
    SS_LOG("Secondary surface destroyed");
}

extern "C" JNIEXPORT void JNICALL Java_com_dishii_soh_DualScreenManager_nativeTouch(JNIEnv*, jclass, jint action,
                                                                                 jfloat x, jfloat y) {
    using namespace Ship::SecondaryScreen;
    std::lock_guard<std::mutex> lock(sState.mutex);
    if (sState.touches.size() < 256) {
        sState.touches.push_back({ action, x, y });
    }
}

#else // !__ANDROID__

namespace Ship {
namespace SecondaryScreen {

void NewFrame() {
    sPrimarySize = ImGui::GetIO().DisplaySize;
    sFrameActive = false;
}

void AfterNewFrame() {
}

bool RenderDrawData(ImDrawData*, void (*)(ImDrawData*)) {
    return false;
}

} // namespace SecondaryScreen
} // namespace Ship

#endif
