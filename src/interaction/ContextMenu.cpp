#include "ContextMenu.h"

#include "model/NodeClass.h"

#include <algorithm>
#include <cctype>

// Rows scrolled per wheel notch.
static const float SCROLL_ROWS_PER_NOTCH = 3.0f;

static std::string ToLowerAscii(const std::string& text)
{
    std::string result = text;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

bool ContextMenu::IsCategoryCollapsed(const std::string& category) const
{
    for (const std::string& collapsed : collapsedCategories) {
        if (collapsed == category) {
            return true;
        }
    }
    return false;
}

void ContextMenu::SetCollapsedCategories(std::vector<std::string> categories)
{
    collapsedCategories = std::move(categories);
    if (open) {
        UpdateFilter();
    }
}

void ContextMenu::ToggleCategoryCollapsed(const std::string& category)
{
    for (std::size_t i = 0; i < collapsedCategories.size(); ++i) {
        if (collapsedCategories[i] == category) {
            collapsedCategories.erase(collapsedCategories.begin()
                                      + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
    collapsedCategories.push_back(category);
}

void ContextMenu::Open(float screenX, float screenY, float canvasX, float canvasY,
                       float screenWidth, float screenHeight)
{
    open = true;
    spawnCanvasX = canvasX;
    spawnCanvasY = canvasY;
    screenHeightAtOpen = screenHeight;
    scrollOffset = 0.0f;
    searchText.clear();
    hoveredIndex = -1;
    UpdateFilter();

    panelX = screenX;
    panelY = screenY;
    const float panelHeight = GetPanelHeight();
    if (panelX + PANEL_WIDTH > screenWidth) {
        panelX = screenWidth - PANEL_WIDTH;
    }
    if (panelY + panelHeight > screenHeight) {
        panelY = screenHeight - panelHeight;
    }
    if (panelX < 0.0f) {
        panelX = 0.0f;
    }
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
}

void ContextMenu::Close()
{
    open = false;
    searchText.clear();
    rows.clear();
    hoveredIndex = -1;
    scrollOffset = 0.0f;
}

float ContextMenu::GetListContentHeight() const
{
    const float rowCount = static_cast<float>(rows.empty() ? 1 : rows.size());
    return rowCount * ITEM_HEIGHT;
}

float ContextMenu::GetPanelHeight() const
{
    return PADDING + SEARCH_HEIGHT + PADDING + listViewHeight + PADDING;
}

ContextMenuAction ContextMenu::HandleEvent(const EditorInputEvent& event)
{
    ContextMenuAction action;
    if (!open) {
        return action;
    }

    switch (event.type) {
    case EditorInputType::MouseMove:
        lastMouseX = event.x;
        lastMouseY = event.y;
        UpdateHover(event.x, event.y);
        break;

    case EditorInputType::MouseDown:
        if (!IsInsidePanel(event.x, event.y)) {
            Close();
            action.type = ContextMenuAction::Type::Close;
            break;
        }
        if (event.button == EditorMouseButton::Left) {
            const int index = RowIndexAt(event.x, event.y);
            if (index >= 0) {
                const ContextMenuRow& row = rows[static_cast<std::size_t>(index)];
                switch (row.kind) {
                case ContextMenuRowKind::CategoryHeader:
                    ToggleCategoryCollapsed(row.category);
                    UpdateFilter();
                    UpdateHover(event.x, event.y);
                    break;
                case ContextMenuRowKind::NodeItem:
                    if (row.nodeClass != nullptr) {
                        const bool inEditZone =
                            event.x >= panelX + PANEL_WIDTH - PADDING - EDIT_ZONE_WIDTH;
                        if (inEditZone && row.nodeClass->IsDynamic()) {
                            action.type = ContextMenuAction::Type::EditClass;
                        } else {
                            action.type = ContextMenuAction::Type::CreateNode;
                        }
                        action.nodeClass = row.nodeClass;
                        Close();
                    }
                    break;
                case ContextMenuRowKind::Paste:
                    action.type = ContextMenuAction::Type::Paste;
                    Close();
                    break;
                case ContextMenuRowKind::AddComment:
                    action.type = ContextMenuAction::Type::AddComment;
                    Close();
                    break;
                case ContextMenuRowKind::CreateNewClass:
                    action.type = ContextMenuAction::Type::OpenClassEditor;
                    Close();
                    break;
                }
            }
        }
        break;

    case EditorInputType::MouseWheel:
        scrollOffset -= event.wheelDelta * ITEM_HEIGHT * SCROLL_ROWS_PER_NOTCH;
        ClampScroll();
        UpdateHover(lastMouseX, lastMouseY);
        break;

    case EditorInputType::KeyDown:
        if (event.key == EditorKey::Escape) {
            Close();
            action.type = ContextMenuAction::Type::Close;
        } else if (event.key == EditorKey::Backspace) {
            RemoveLastSearchCharacter();
            UpdateFilter();
        } else if (event.key == EditorKey::Enter) {
            const NodeClass* firstClass = FirstItemClass();
            if (firstClass != nullptr) {
                action.type = ContextMenuAction::Type::CreateNode;
                action.nodeClass = firstClass;
                Close();
            }
        }
        break;

    case EditorInputType::TextInput:
        searchText += event.text;
        UpdateFilter();
        break;

    case EditorInputType::MouseUp:
        break;
    }

    return action;
}

void ContextMenu::UpdateFilter()
{
    rows.clear();
    const std::string needle = ToLowerAscii(searchText);
    const bool searching = !needle.empty();

    // Distinct categories: builtin names first (canonical order), then
    // user-defined categories in registry order.
    std::vector<std::string> categories;
    for (const char* builtinName : BUILTIN_CATEGORY_NAMES) {
        for (const NodeClass* nodeClass : NodeClass::GetRegistry()) {
            if (nodeClass->GetCategory() == builtinName) {
                categories.push_back(builtinName);
                break;
            }
        }
    }
    for (const NodeClass* nodeClass : NodeClass::GetRegistry()) {
        bool known = false;
        for (const std::string& existing : categories) {
            if (existing == nodeClass->GetCategory()) {
                known = true;
                break;
            }
        }
        if (!known) {
            categories.push_back(nodeClass->GetCategory());
        }
    }

    for (const std::string& category : categories) {
        bool headerAdded = false;
        for (const NodeClass* nodeClass : NodeClass::GetRegistry()) {
            if (nodeClass->GetCategory() != category) {
                continue;
            }
            const std::string title = ToLowerAscii(nodeClass->GetName());
            if (searching && title.find(needle) == std::string::npos) {
                continue;
            }
            if (!headerAdded) {
                ContextMenuRow header;
                header.kind = ContextMenuRowKind::CategoryHeader;
                header.category = category;
                rows.push_back(header);
                headerAdded = true;
            }
            // While searching, collapse is ignored so matches stay visible.
            if (!searching && IsCategoryCollapsed(category)) {
                continue;
            }
            ContextMenuRow item;
            item.kind = ContextMenuRowKind::NodeItem;
            item.category = category;
            item.nodeClass = nodeClass;
            rows.push_back(item);
        }
    }

    if (pasteAvailable) {
        ContextMenuRow pasteRow;
        pasteRow.kind = ContextMenuRowKind::Paste;
        rows.push_back(pasteRow);
    }

    ContextMenuRow addCommentRow;
    addCommentRow.kind = ContextMenuRowKind::AddComment;
    rows.push_back(addCommentRow);

    ContextMenuRow createNewRow;
    createNewRow.kind = ContextMenuRowKind::CreateNewClass;
    rows.push_back(createNewRow);

    hoveredIndex = -1;
    UpdateListViewHeight();
    ClampScroll();
}

void ContextMenu::UpdateListViewHeight()
{
    const float chromeHeight = PADDING + SEARCH_HEIGHT + PADDING + PADDING;
    listViewHeight = std::min(GetListContentHeight(), MAX_LIST_HEIGHT);
    if (screenHeightAtOpen > chromeHeight + ITEM_HEIGHT) {
        listViewHeight = std::min(listViewHeight, screenHeightAtOpen - chromeHeight);
    }
}

void ContextMenu::ClampScroll()
{
    const float maxScroll = std::max(0.0f, GetListContentHeight() - listViewHeight);
    scrollOffset = std::max(0.0f, std::min(scrollOffset, maxScroll));
}

void ContextMenu::UpdateHover(float x, float y)
{
    const int index = RowIndexAt(x, y);
    const bool hoverable = index >= 0
        && rows[static_cast<std::size_t>(index)].kind != ContextMenuRowKind::CategoryHeader;
    hoveredIndex = hoverable ? index : -1;
}

bool ContextMenu::IsInsidePanel(float x, float y) const
{
    return x >= panelX && x <= panelX + PANEL_WIDTH
        && y >= panelY && y <= panelY + GetPanelHeight();
}

int ContextMenu::RowIndexAt(float x, float y) const
{
    if (x < panelX || x > panelX + PANEL_WIDTH) {
        return -1;
    }
    const float listTop = panelY + PADDING + SEARCH_HEIGHT + PADDING;
    if (y < listTop || y > listTop + listViewHeight) {
        return -1;
    }
    const int index = static_cast<int>((y - listTop + scrollOffset) / ITEM_HEIGHT);
    if (index >= static_cast<int>(rows.size())) {
        return -1;
    }
    return index;
}

const NodeClass* ContextMenu::FirstItemClass() const
{
    for (const ContextMenuRow& row : rows) {
        if (row.kind == ContextMenuRowKind::NodeItem && row.nodeClass != nullptr) {
            return row.nodeClass;
        }
    }
    return nullptr;
}

void ContextMenu::RemoveLastSearchCharacter()
{
    // Pop one UTF-8 code point: drop continuation bytes, then the lead byte.
    while (!searchText.empty()
           && (static_cast<unsigned char>(searchText.back()) & 0xC0) == 0x80) {
        searchText.pop_back();
    }
    if (!searchText.empty()) {
        searchText.pop_back();
    }
}
