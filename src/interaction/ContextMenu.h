#pragma once

#include "EditorInputEvent.h"

#include "model/GraphTypes.h"

#include <string>
#include <vector>

class NodeClass;

struct ContextMenuAction
{
    enum class Type
    {
        None,
        Close,
        CreateNode,
        // "+ Create New Class..." was chosen; the caller opens the
        // class editor dialog.
        OpenClassEditor,
    };

    Type type = Type::None;
    // Class to instantiate when type == CreateNode.
    const NodeClass* nodeClass = nullptr;
};

enum class ContextMenuRowKind
{
    CategoryHeader,
    NodeItem,
    CreateNewClass,
};

// One display row of the menu list.
struct ContextMenuRow
{
    ContextMenuRowKind kind = ContextMenuRowKind::NodeItem;
    NodeCategory category = NodeCategory::Function;
    // Set only for NodeItem rows.
    const NodeClass* nodeClass = nullptr;
};

// Blueprint-style node creation menu: opened by a right click on empty
// canvas (or Tab), filters the NodeClass registry by a typed search
// string and groups results under collapsible category headers. The list
// scrolls when taller than MAX_LIST_HEIGHT. Collapse state persists
// across opens; while searching, collapse is ignored so matches are
// always visible. Owns menu state only; drawing lives in
// render/ContextMenuRenderer.
class ContextMenu
{
public:
    static constexpr float PANEL_WIDTH = 240.0f;
    static constexpr float SEARCH_HEIGHT = 28.0f;
    static constexpr float ITEM_HEIGHT = 22.0f;
    static constexpr float PADDING = 6.0f;
    static constexpr float MAX_LIST_HEIGHT = 320.0f;

    bool IsOpen() const { return open; }

    // screenX/Y: where the panel opens (clamped to the window).
    // canvasX/Y: canvas position where a created node will be spawned.
    void Open(float screenX, float screenY, float canvasX, float canvasY,
              float screenWidth, float screenHeight);
    void Close();

    // Handles one input event while open. The menu consumes all events;
    // the returned action tells the caller what happened.
    ContextMenuAction HandleEvent(const EditorInputEvent& event);

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    const std::string& GetSearchText() const { return searchText; }
    const std::vector<ContextMenuRow>& GetRows() const { return rows; }
    // Index into GetRows() of the hovered item row, or -1. Header rows
    // are never hovered.
    int GetHoveredIndex() const { return hoveredIndex; }
    bool IsCategoryCollapsed(NodeCategory category) const;
    // Applies persisted collapse state (e.g. from EditorSettings).
    void SetCategoryCollapsed(NodeCategory category, bool isCollapsed);
    // Scroll offset of the list content in pixels.
    float GetScrollOffset() const { return scrollOffset; }
    // Visible height of the list region.
    float GetListViewHeight() const { return listViewHeight; }
    float GetListContentHeight() const;
    float GetSpawnCanvasX() const { return spawnCanvasX; }
    float GetSpawnCanvasY() const { return spawnCanvasY; }

private:
    void UpdateFilter();
    void UpdateListViewHeight();
    void ClampScroll();
    void UpdateHover(float x, float y);
    bool IsInsidePanel(float x, float y) const;
    // Returns the row index under a screen point, or -1.
    int RowIndexAt(float x, float y) const;
    const NodeClass* FirstItemClass() const;
    void RemoveLastSearchCharacter();

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    float spawnCanvasX = 0.0f;
    float spawnCanvasY = 0.0f;
    float screenHeightAtOpen = 0.0f;
    float scrollOffset = 0.0f;
    float listViewHeight = 0.0f;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    std::string searchText;
    std::vector<ContextMenuRow> rows;
    int hoveredIndex = -1;
    // Indexed by CategoryIndex(); persists across menu opens.
    bool collapsed[4] = {false, false, false, false};
};
