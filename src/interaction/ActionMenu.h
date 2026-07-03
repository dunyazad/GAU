#pragma once

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

#include <string>
#include <vector>

struct ActionMenuResult
{
    enum class Type
    {
        None,
        // An item was chosen; itemIndex is its position.
        Selected,
        Closed,
    };

    Type type = Type::None;
    int itemIndex = -1;
};

// Small popup listing actions for a right-clicked target (node/comment).
// Owns state and hit testing only; drawing lives in
// render/ActionMenuRenderer.
class ActionMenu
{
public:
    static constexpr float WIDTH = 160.0f * UI_SCALE;
    static constexpr float ITEM_HEIGHT = 22.0f * UI_SCALE;
    static constexpr float PADDING = 4.0f * UI_SCALE;

    bool IsOpen() const { return open; }

    void Open(float screenX, float screenY, std::vector<std::string> menuItems,
              float screenWidth, float screenHeight);
    void Close();

    // Consumes all events while open.
    ActionMenuResult HandleEvent(const EditorInputEvent& event);

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    const std::vector<std::string>& GetItems() const { return items; }
    int GetHoveredIndex() const { return hoveredIndex; }

private:
    int ItemIndexAt(float x, float y) const;

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    std::vector<std::string> items;
    int hoveredIndex = -1;
};
