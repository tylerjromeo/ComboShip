#pragma once

#include <ship/window/gui/GuiWindow.h>
#include "soh/Enhancements/randomizer/randomizerEnums.h"

typedef enum {
    // Every hint location grouped by area, with unread hint text masked.
    HINT_TRACKER_VIEW_LOCATIONS,
    // Only hints the player has read, grouped by hint type by usefulness.
    HINT_TRACKER_VIEW_JOURNAL,
} HintTrackerViewMode;

namespace HintTracker {

// True once the player has seen this hint's text (thread-safe). Used by the unread-hint markers.
bool IsHintRead(RandomizerHint hintKey);

class HintTrackerSettingsWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

  protected:
    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};

class HintTrackerWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void Draw() override;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override{};
};
} // namespace HintTracker
