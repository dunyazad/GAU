#include "ActionMenu.h"

#include <utility>

void ActionMenu::Open(float screenX, float screenY, std::vector<std::string> menuItems,
                      float screenWidth, float screenHeight)
{
    open = true;
    items = std::move(menuItems);
    hoveredIndex = -1;

    panelX = screenX;
    panelY = screenY;
    if (panelX + WIDTH > screenWidth) {
        panelX = screenWidth - WIDTH;
    }
    if (panelY + GetPanelHeight() > screenHeight) {
        panelY = screenHeight - GetPanelHeight();
    }
    if (panelX < 0.0f) {
        panelX = 0.0f;
    }
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
}

void ActionMenu::Close()
{
    open = false;
    items.clear();
    hoveredIndex = -1;
}

float ActionMenu::GetPanelHeight() const
{
    return PADDING * 2.0f + ITEM_HEIGHT * static_cast<float>(items.size());
}

int ActionMenu::ItemIndexAt(float x, float y) const
{
    if (x < panelX || x > panelX + WIDTH) {
        return -1;
    }
    const float listTop = panelY + PADDING;
    if (y < listTop) {
        return -1;
    }
    const int index = static_cast<int>((y - listTop) / ITEM_HEIGHT);
    if (index >= static_cast<int>(items.size())) {
        return -1;
    }
    return index;
}

ActionMenuResult ActionMenu::HandleEvent(const EditorInputEvent& event)
{
    ActionMenuResult result;
    if (!open) {
        return result;
    }

    switch (event.type) {
    case EditorInputType::MouseMove:
        hoveredIndex = ItemIndexAt(event.x, event.y);
        break;

    case EditorInputType::MouseDown: {
        const int index = ItemIndexAt(event.x, event.y);
        if (event.button == EditorMouseButton::Left && index >= 0) {
            result.type = ActionMenuResult::Type::Selected;
            result.itemIndex = index;
        } else {
            result.type = ActionMenuResult::Type::Closed;
        }
        Close();
        break;
    }

    case EditorInputType::KeyDown:
        if (event.key == EditorKey::Escape) {
            result.type = ActionMenuResult::Type::Closed;
            Close();
        }
        break;

    case EditorInputType::MouseUp:
    case EditorInputType::MouseWheel:
    case EditorInputType::TextInput:
        break;
    }
    return result;
}
