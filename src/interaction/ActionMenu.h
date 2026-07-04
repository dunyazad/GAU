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
        // An item was chosen.
        Selected,
        Closed,
    };

    Type type = Type::None;
    // Chosen top-level item, valid when submenuParent < 0.
    int itemIndex = -1;
    // When >= 0, a submenu item was chosen: submenuParent is the parent
    // item index and submenuIndex is the position within its submenu.
    int submenuParent = -1;
    int submenuIndex = -1;
};

// Small popup listing actions for a right-clicked target (node/comment).
// Supports one level of hover-activated submenu (e.g. "Align"). Owns state
// and hit testing only; drawing lives in render/ActionMenuRenderer.
class ActionMenu
{
public:
    static constexpr float WIDTH = 160.0f * UI_SCALE;
    static constexpr float ITEM_HEIGHT = 22.0f * UI_SCALE;
    static constexpr float PADDING = 4.0f * UI_SCALE;
    // Vertical space a divider occupies between two item groups.
    static constexpr float SEPARATOR_HEIGHT = 7.0f * UI_SCALE;

    bool IsOpen() const { return open; }

    // separatorAfterItems: top-level item indices after which a divider is
    // drawn. Dividers are visual only and never change item indices.
    void Open(float screenX, float screenY, std::vector<std::string> menuItems,
              float screenWidth, float screenHeight,
              std::vector<int> separatorAfterItems = {});
    // Attaches a hover-activated submenu to a top-level item. Call after
    // Open. subSeparators groups the submenu items like separatorAfterItems.
    void SetSubmenu(int itemIndex, std::vector<std::string> subItems,
                    std::vector<int> subSeparators = {});
    void Close();

    // Consumes all events while open.
    ActionMenuResult HandleEvent(const EditorInputEvent& event);

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    const std::vector<std::string>& GetItems() const { return items; }
    int GetHoveredIndex() const { return hoveredIndex; }
    bool IsSeparatorAfter(int index) const;
    float ItemOffsetY(int index) const;
    bool ItemHasSubmenu(int index) const;

    // Submenu accessors (for the renderer). GetOpenSubmenu() < 0 means no
    // submenu is currently shown.
    int GetOpenSubmenu() const { return openSubIndex; }
    const std::vector<std::string>& GetSubmenuItems() const;
    int GetSubmenuHoveredIndex() const { return hoveredSubItem; }
    float GetSubmenuX() const { return submenuX; }
    float GetSubmenuY() const { return submenuY; }
    float GetSubmenuPanelHeight() const;
    bool IsSubmenuSeparatorAfter(int index) const;
    float SubmenuItemOffsetY(int index) const;

private:
    int ItemIndexAt(float x, float y) const;
    int SubItemIndexAt(float x, float y) const;
    bool PointInSubmenu(float x, float y) const;
    void OpenSubmenuFor(int parentIndex);
    static float OffsetY(const std::vector<int>& separators, int index);

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    float screenWidthAtOpen = 0.0f;
    float screenHeightAtOpen = 0.0f;
    std::vector<std::string> items;
    std::vector<int> separatorAfter;
    // Parallel to items: submenus[i] is item i's submenu (empty = none).
    std::vector<std::vector<std::string>> submenus;
    std::vector<std::vector<int>> submenuSeparators;
    int hoveredIndex = -1;

    // Currently shown submenu's parent index, or -1.
    int openSubIndex = -1;
    int hoveredSubItem = -1;
    float submenuX = 0.0f;
    float submenuY = 0.0f;
};
