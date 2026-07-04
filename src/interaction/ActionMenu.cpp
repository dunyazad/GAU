#include "ActionMenu.h"

#include <utility>

void ActionMenu::Open(float screenX, float screenY, std::vector<std::string> menuItems,
                      float screenWidth, float screenHeight,
                      std::vector<int> separatorAfterItems)
{
    open = true;
    items = std::move(menuItems);
    separatorAfter = std::move(separatorAfterItems);
    submenus.assign(items.size(), {});
    submenuSeparators.assign(items.size(), {});
    hoveredIndex = -1;
    openSubIndex = -1;
    hoveredSubItem = -1;
    screenWidthAtOpen = screenWidth;
    screenHeightAtOpen = screenHeight;

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

void ActionMenu::SetSubmenu(int itemIndex, std::vector<std::string> subItems,
                            std::vector<int> subSeparators)
{
    if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size())) {
        return;
    }
    submenus[static_cast<std::size_t>(itemIndex)] = std::move(subItems);
    submenuSeparators[static_cast<std::size_t>(itemIndex)] = std::move(subSeparators);
}

void ActionMenu::Close()
{
    open = false;
    items.clear();
    separatorAfter.clear();
    submenus.clear();
    submenuSeparators.clear();
    hoveredIndex = -1;
    openSubIndex = -1;
    hoveredSubItem = -1;
}

float ActionMenu::OffsetY(const std::vector<int>& separators, int index)
{
    float offset = 0.0f;
    for (int i = 0; i < index; ++i) {
        offset += ITEM_HEIGHT;
        for (int after : separators) {
            if (after == i) {
                offset += SEPARATOR_HEIGHT;
                break;
            }
        }
    }
    return offset;
}

bool ActionMenu::IsSeparatorAfter(int index) const
{
    for (int after : separatorAfter) {
        if (after == index) {
            return true;
        }
    }
    return false;
}

float ActionMenu::ItemOffsetY(int index) const
{
    return OffsetY(separatorAfter, index);
}

bool ActionMenu::ItemHasSubmenu(int index) const
{
    if (index < 0 || index >= static_cast<int>(submenus.size())) {
        return false;
    }
    return !submenus[static_cast<std::size_t>(index)].empty();
}

const std::vector<std::string>& ActionMenu::GetSubmenuItems() const
{
    static const std::vector<std::string> empty;
    if (openSubIndex < 0 || openSubIndex >= static_cast<int>(submenus.size())) {
        return empty;
    }
    return submenus[static_cast<std::size_t>(openSubIndex)];
}

bool ActionMenu::IsSubmenuSeparatorAfter(int index) const
{
    if (openSubIndex < 0 || openSubIndex >= static_cast<int>(submenuSeparators.size())) {
        return false;
    }
    for (int after : submenuSeparators[static_cast<std::size_t>(openSubIndex)]) {
        if (after == index) {
            return true;
        }
    }
    return false;
}

float ActionMenu::SubmenuItemOffsetY(int index) const
{
    if (openSubIndex < 0 || openSubIndex >= static_cast<int>(submenuSeparators.size())) {
        return ITEM_HEIGHT * static_cast<float>(index);
    }
    return OffsetY(submenuSeparators[static_cast<std::size_t>(openSubIndex)], index);
}

float ActionMenu::GetPanelHeight() const
{
    return PADDING * 2.0f + ItemOffsetY(static_cast<int>(items.size()));
}

float ActionMenu::GetSubmenuPanelHeight() const
{
    return PADDING * 2.0f + SubmenuItemOffsetY(static_cast<int>(GetSubmenuItems().size()));
}

void ActionMenu::OpenSubmenuFor(int parentIndex)
{
    openSubIndex = parentIndex;
    hoveredSubItem = -1;
    const float parentItemTop = panelY + PADDING + ItemOffsetY(parentIndex);

    // Prefer the right side; flip left when it would overflow the window.
    submenuX = panelX + WIDTH - 2.0f * UI_SCALE;
    if (submenuX + WIDTH > screenWidthAtOpen) {
        submenuX = panelX - WIDTH + 2.0f * UI_SCALE;
    }
    if (submenuX < 0.0f) {
        submenuX = 0.0f;
    }

    submenuY = parentItemTop;
    const float subHeight = GetSubmenuPanelHeight();
    if (submenuY + subHeight > screenHeightAtOpen) {
        submenuY = screenHeightAtOpen - subHeight;
    }
    if (submenuY < 0.0f) {
        submenuY = 0.0f;
    }
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
    const float localY = y - listTop;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const float itemTop = ItemOffsetY(i);
        if (localY >= itemTop && localY < itemTop + ITEM_HEIGHT) {
            return i;
        }
    }
    return -1;
}

bool ActionMenu::PointInSubmenu(float x, float y) const
{
    if (openSubIndex < 0) {
        return false;
    }
    return x >= submenuX && x <= submenuX + WIDTH && y >= submenuY
        && y <= submenuY + GetSubmenuPanelHeight();
}

int ActionMenu::SubItemIndexAt(float x, float y) const
{
    if (!PointInSubmenu(x, y)) {
        return -1;
    }
    const float localY = y - (submenuY + PADDING);
    if (localY < 0.0f) {
        return -1;
    }
    const std::vector<std::string>& subItems = GetSubmenuItems();
    for (int i = 0; i < static_cast<int>(subItems.size()); ++i) {
        const float itemTop = SubmenuItemOffsetY(i);
        if (localY >= itemTop && localY < itemTop + ITEM_HEIGHT) {
            return i;
        }
    }
    return -1;
}

ActionMenuResult ActionMenu::HandleEvent(const EditorInputEvent& event)
{
    ActionMenuResult result;
    if (!open) {
        return result;
    }

    switch (event.type) {
    case EditorInputType::MouseMove:
        if (PointInSubmenu(event.x, event.y)) {
            // Keep the parent item highlighted while over its submenu.
            hoveredSubItem = SubItemIndexAt(event.x, event.y);
        } else {
            hoveredIndex = ItemIndexAt(event.x, event.y);
            hoveredSubItem = -1;
            if (ItemHasSubmenu(hoveredIndex)) {
                if (openSubIndex != hoveredIndex) {
                    OpenSubmenuFor(hoveredIndex);
                }
            } else {
                openSubIndex = -1;
            }
        }
        break;

    case EditorInputType::MouseDown: {
        if (event.button != EditorMouseButton::Left) {
            result.type = ActionMenuResult::Type::Closed;
            Close();
            break;
        }
        const int subIndex = SubItemIndexAt(event.x, event.y);
        if (subIndex >= 0) {
            result.type = ActionMenuResult::Type::Selected;
            result.submenuParent = openSubIndex;
            result.submenuIndex = subIndex;
            Close();
            break;
        }
        const int index = ItemIndexAt(event.x, event.y);
        if (index >= 0) {
            if (ItemHasSubmenu(index)) {
                // Clicking a submenu parent just keeps its flyout open.
                OpenSubmenuFor(index);
                break;
            }
            result.type = ActionMenuResult::Type::Selected;
            result.itemIndex = index;
            Close();
            break;
        }
        result.type = ActionMenuResult::Type::Closed;
        Close();
        break;
    }

    case EditorInputType::KeyDown:
        if (event.key == EditorKey::Escape) {
            if (openSubIndex >= 0) {
                openSubIndex = -1;
                hoveredSubItem = -1;
            } else {
                result.type = ActionMenuResult::Type::Closed;
                Close();
            }
        }
        break;

    case EditorInputType::MouseUp:
    case EditorInputType::MouseWheel:
    case EditorInputType::TextInput:
        break;
    }
    return result;
}
