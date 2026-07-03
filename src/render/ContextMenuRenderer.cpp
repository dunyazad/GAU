#include "ContextMenuRenderer.h"
#include "CategoryStyle.h"

#include "interaction/ContextMenu.h"
#include "model/NodeClass.h"

#include <nanovg.h>

static const char* FONT_REGULAR = "sans";
static const float MENU_FONT_SIZE = 13.0f * UI_SCALE;

static void DrawSearchBox(NVGcontext* vg, const ContextMenu& menu, float x, float y, float width)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, ContextMenu::SEARCH_HEIGHT, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(15, 15, 17));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, MENU_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    const float textX = x + 8.0f * UI_SCALE;
    const float textY = y + ContextMenu::SEARCH_HEIGHT * 0.5f;
    const std::string& searchText = menu.GetSearchText();
    if (searchText.empty()) {
        nvgFillColor(vg, nvgRGB(120, 120, 128));
        nvgText(vg, textX, textY, "Search...", nullptr);
    } else {
        nvgFillColor(vg, nvgRGB(235, 235, 240));
        const std::string withCaret = searchText + "|";
        nvgText(vg, textX, textY, withCaret.c_str(), nullptr);
    }
}

// Arrow indicating collapse state: right-pointing when collapsed,
// down-pointing when expanded.
static void DrawCollapseArrow(NVGcontext* vg, float x, float centerY, bool isCollapsed)
{
    nvgBeginPath(vg);
    if (isCollapsed) {
        nvgMoveTo(vg, x, centerY - 4.0f * UI_SCALE);
        nvgLineTo(vg, x, centerY + 4.0f * UI_SCALE);
        nvgLineTo(vg, x + 5.0f * UI_SCALE, centerY);
    } else {
        nvgMoveTo(vg, x - 1.0f * UI_SCALE, centerY - 2.0f * UI_SCALE);
        nvgLineTo(vg, x + 7.0f * UI_SCALE, centerY - 2.0f * UI_SCALE);
        nvgLineTo(vg, x + 3.0f * UI_SCALE, centerY + 3.0f * UI_SCALE);
    }
    nvgClosePath(vg);
    nvgFillColor(vg, nvgRGB(150, 150, 158));
    nvgFill(vg);
}

static void DrawHeaderRow(NVGcontext* vg, const ContextMenu& menu, const ContextMenuRow& row,
                          float x, float centerY)
{
    DrawCollapseArrow(vg, x + 5.0f * UI_SCALE, centerY, menu.IsCategoryCollapsed(row.category));

    nvgBeginPath(vg);
    nvgCircle(vg, x + 20.0f * UI_SCALE, centerY, 3.0f * UI_SCALE);
    nvgFillColor(vg, CategoryColor(row.category));
    nvgFill(vg);

    nvgFillColor(vg, nvgRGB(150, 150, 158));
    nvgText(vg, x + 28.0f * UI_SCALE, centerY, CategoryDisplayName(row.category).c_str(), nullptr);
}

static void DrawScrollbar(NVGcontext* vg, const ContextMenu& menu, float panelRight, float listTop)
{
    const float contentHeight = menu.GetListContentHeight();
    const float viewHeight = menu.GetListViewHeight();
    if (contentHeight <= viewHeight) {
        return;
    }

    const float trackWidth = 3.0f * UI_SCALE;
    const float trackX = panelRight - trackWidth - 2.0f * UI_SCALE;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, trackX, listTop, trackWidth, viewHeight, trackWidth * 0.5f);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 20));
    nvgFill(vg);

    float thumbHeight = viewHeight * viewHeight / contentHeight;
    const float minThumbHeight = 20.0f * UI_SCALE;
    if (thumbHeight < minThumbHeight) {
        thumbHeight = minThumbHeight;
    }
    const float maxScroll = contentHeight - viewHeight;
    const float thumbTravel = viewHeight - thumbHeight;
    const float thumbY = listTop + (menu.GetScrollOffset() / maxScroll) * thumbTravel;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, trackX, thumbY, trackWidth, thumbHeight, trackWidth * 0.5f);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 90));
    nvgFill(vg);
}

static void DrawMenuItems(NVGcontext* vg, const ContextMenu& menu, float x, float listTop, float width)
{
    const std::vector<ContextMenuRow>& rows = menu.GetRows();
    const float viewHeight = menu.GetListViewHeight();
    const float scroll = menu.GetScrollOffset();

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, MENU_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    if (rows.empty()) {
        nvgFillColor(vg, nvgRGB(120, 120, 128));
        nvgText(vg, x + 8.0f * UI_SCALE, listTop + ContextMenu::ITEM_HEIGHT * 0.5f,
                "No results", nullptr);
        return;
    }

    nvgSave(vg);
    nvgIntersectScissor(vg, x, listTop, width, viewHeight);

    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const float rowY = listTop + ContextMenu::ITEM_HEIGHT * static_cast<float>(i) - scroll;
        if (rowY + ContextMenu::ITEM_HEIGHT < listTop || rowY > listTop + viewHeight) {
            continue;
        }

        const ContextMenuRow& row = rows[static_cast<std::size_t>(i)];
        const float centerY = rowY + ContextMenu::ITEM_HEIGHT * 0.5f;

        if (row.kind == ContextMenuRowKind::CategoryHeader) {
            DrawHeaderRow(vg, menu, row, x, centerY);
            continue;
        }

        if (i == menu.GetHoveredIndex()) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x + 2.0f * UI_SCALE, rowY, width - 4.0f * UI_SCALE,
                           ContextMenu::ITEM_HEIGHT, 2.0f * UI_SCALE);
            nvgFillColor(vg, nvgRGBA(70, 110, 180, 220));
            nvgFill(vg);
        }

        if (row.kind == ContextMenuRowKind::Paste
            || row.kind == ContextMenuRowKind::AddComment
            || row.kind == ContextMenuRowKind::CreateNewClass
            || row.kind == ContextMenuRowKind::NewWasmFunction) {
            // Separator above the first special row.
            const bool firstSpecial =
                (i == 0)
                || (rows[static_cast<std::size_t>(i - 1)].kind == ContextMenuRowKind::CategoryHeader
                    || rows[static_cast<std::size_t>(i - 1)].kind == ContextMenuRowKind::NodeItem);
            if (firstSpecial) {
                nvgBeginPath(vg);
                nvgMoveTo(vg, x + 4.0f * UI_SCALE, rowY + 0.5f);
                nvgLineTo(vg, x + width - 4.0f * UI_SCALE, rowY + 0.5f);
                nvgStrokeColor(vg, nvgRGB(60, 60, 66));
                nvgStrokeWidth(vg, 1.0f);
                nvgStroke(vg);
            }

            const char* label = "+ Create New Class...";
            if (row.kind == ContextMenuRowKind::Paste) {
                label = "Paste";
            } else if (row.kind == ContextMenuRowKind::AddComment) {
                label = "+ Add Comment";
            } else if (row.kind == ContextMenuRowKind::NewWasmFunction) {
                label = "+ New Wasm Function...";
            }
            nvgFillColor(vg, nvgRGB(140, 180, 240));
            nvgText(vg, x + 8.0f * UI_SCALE, centerY, label, nullptr);
            continue;
        }

        nvgFillColor(vg, nvgRGB(225, 225, 230));
        nvgText(vg, x + 28.0f * UI_SCALE, centerY, row.nodeClass->GetName(), nullptr);

        // Dynamic classes show an edit affordance while hovered.
        if (i == menu.GetHoveredIndex() && row.nodeClass->IsDynamic()) {
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(180, 200, 230));
            nvgText(vg, x + width - 8.0f * UI_SCALE, centerY, "edit", nullptr);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }

    nvgRestore(vg);
}

void DrawContextMenu(NVGcontext* vg, const ContextMenu& menu)
{
    if (!menu.IsOpen()) {
        return;
    }

    const float x = menu.GetX();
    const float y = menu.GetY();
    const float width = ContextMenu::PANEL_WIDTH;
    const float height = menu.GetPanelHeight();

    // Panel background.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 4.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(20, 20, 22, 245));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    const float innerX = x + ContextMenu::PADDING;
    const float innerWidth = width - ContextMenu::PADDING * 2.0f;
    DrawSearchBox(vg, menu, innerX, y + ContextMenu::PADDING, innerWidth);

    const float listTop = y + ContextMenu::PADDING + ContextMenu::SEARCH_HEIGHT + ContextMenu::PADDING;
    DrawMenuItems(vg, menu, innerX, listTop, innerWidth);
    DrawScrollbar(vg, menu, x + width, listTop);
}
