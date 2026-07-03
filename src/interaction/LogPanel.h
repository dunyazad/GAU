#pragma once

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

// Collapsible output log strip along the bottom of the window. The log
// lines themselves live in main (shared with the console sink and
// future runtime views); this owns only view state and hit testing.
class LogPanel
{
public:
    static constexpr float HEADER_HEIGHT = 24.0f * UI_SCALE;
    static constexpr float BODY_HEIGHT = 140.0f * UI_SCALE;
    static constexpr float LINE_HEIGHT = 18.0f * UI_SCALE;
    static constexpr float PADDING = 6.0f * UI_SCALE;
    static constexpr float CLEAR_WIDTH = 56.0f * UI_SCALE;

    bool IsCollapsed() const { return collapsed; }
    // Lines scrolled up from the bottom of the log.
    int GetScrollLines() const { return scrollLines; }
    int VisibleLineCount() const { return static_cast<int>(BODY_HEIGHT / LINE_HEIGHT); }

    float CurrentHeight() const { return collapsed ? HEADER_HEIGHT : HEADER_HEIGHT + BODY_HEIGHT; }
    float GetTop(float screenHeight) const { return screenHeight - CurrentHeight(); }

    UIRect HeaderRect(float screenWidth, float screenHeight) const;
    UIRect ClearButtonRect(float screenWidth, float screenHeight) const;
    UIRect BodyRect(float screenWidth, float screenHeight) const;

    // Returns true when consumed; outClear is set when Clear was clicked.
    bool HandleEvent(const EditorInputEvent& event, int lineCount,
                     float screenWidth, float screenHeight, bool& outClear);

private:
    bool collapsed = false;
    int scrollLines = 0;
};
