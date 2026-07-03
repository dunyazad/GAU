#pragma once

#include "UIRect.h"
#include "UIScale.h"

// Document tab bar across the top of the window: one tab per open
// graph, a "+" button, and Open/Save/Save As buttons on the right.
// Pure layout/hit-test helpers shared with render/TabBarRenderer;
// document state lives in main.

constexpr float TAB_BAR_HEIGHT = 30.0f * UI_SCALE;
constexpr float TAB_WIDTH = 150.0f * UI_SCALE;
constexpr float TAB_CLOSE_WIDTH = 22.0f * UI_SCALE;
constexpr float TAB_NEW_WIDTH = 28.0f * UI_SCALE;
constexpr float TAB_BUTTON_MARGIN = 4.0f * UI_SCALE;
constexpr float TAB_OPEN_SAVE_WIDTH = 64.0f * UI_SCALE;
constexpr float TAB_SAVE_AS_WIDTH = 92.0f * UI_SCALE;

struct TabBarHit
{
    enum class Kind
    {
        None,
        Tab,
        CloseTab,
        NewTab,
        Open,
        Save,
        SaveAs,
    };

    Kind kind = Kind::None;
    int tabIndex = -1;
};

UIRect TabRect(int tabIndex);
UIRect TabCloseRect(int tabIndex);
UIRect NewTabRect(int tabCount);
UIRect OpenButtonRect(float screenWidth);
UIRect SaveButtonRect(float screenWidth);
UIRect SaveAsButtonRect(float screenWidth);

TabBarHit HitTestTabBar(float x, float y, int tabCount, float screenWidth);
