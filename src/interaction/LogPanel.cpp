#include "LogPanel.h"

UIRect LogPanel::HeaderRect(float screenWidth, float screenHeight) const
{
    return UIRect{0.0f, GetTop(screenHeight), screenWidth, HEADER_HEIGHT};
}

UIRect LogPanel::ClearButtonRect(float screenWidth, float screenHeight) const
{
    const UIRect header = HeaderRect(screenWidth, screenHeight);
    return UIRect{screenWidth - CLEAR_WIDTH - PADDING, header.y + 2.0f * UI_SCALE,
                  CLEAR_WIDTH, HEADER_HEIGHT - 4.0f * UI_SCALE};
}

UIRect LogPanel::BodyRect(float screenWidth, float screenHeight) const
{
    return UIRect{0.0f, GetTop(screenHeight) + HEADER_HEIGHT, screenWidth, BODY_HEIGHT};
}

bool LogPanel::HandleEvent(const EditorInputEvent& event, int lineCount,
                           float screenWidth, float screenHeight, bool& outClear)
{
    outClear = false;

    switch (event.type) {
    case EditorInputType::MouseDown:
        if (ClearButtonRect(screenWidth, screenHeight).Contains(event.x, event.y)) {
            if (event.button == EditorMouseButton::Left) {
                outClear = true;
                scrollLines = 0;
            }
            return true;
        }
        if (HeaderRect(screenWidth, screenHeight).Contains(event.x, event.y)) {
            if (event.button == EditorMouseButton::Left) {
                collapsed = !collapsed;
            }
            return true;
        }
        if (!collapsed && BodyRect(screenWidth, screenHeight).Contains(event.x, event.y)) {
            return true;
        }
        return false;

    case EditorInputType::MouseWheel:
        if (!collapsed && BodyRect(screenWidth, screenHeight).Contains(event.x, event.y)) {
            scrollLines += static_cast<int>(event.wheelDelta) * 3;
            const int maxScroll = (lineCount > VisibleLineCount())
                                      ? lineCount - VisibleLineCount()
                                      : 0;
            if (scrollLines < 0) {
                scrollLines = 0;
            }
            if (scrollLines > maxScroll) {
                scrollLines = maxScroll;
            }
            return true;
        }
        return false;

    case EditorInputType::MouseMove:
    case EditorInputType::MouseUp:
    case EditorInputType::KeyDown:
    case EditorInputType::TextInput:
        break;
    }
    return false;
}
