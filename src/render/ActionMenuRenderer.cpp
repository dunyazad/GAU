#include "ActionMenuRenderer.h"

#include "interaction/ActionMenu.h"

#include <nanovg.h>

static const char* FONT_REGULAR = "sans";
static const float MENU_FONT_SIZE = 13.0f * UI_SCALE;

static void DrawPanelBackground(NVGcontext* vg, float x, float y, float width, float height)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 4.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(20, 20, 22, 245));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}

static void DrawItem(NVGcontext* vg, float x, float itemY, float width, const char* label,
                     bool hovered, bool hasSubmenu, bool separatorBelow)
{
    if (hovered) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x + 2.0f * UI_SCALE, itemY, width - 4.0f * UI_SCALE,
                       ActionMenu::ITEM_HEIGHT, 2.0f * UI_SCALE);
        nvgFillColor(vg, nvgRGBA(70, 110, 180, 220));
        nvgFill(vg);
    }

    nvgFillColor(vg, nvgRGB(225, 225, 230));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + 8.0f * UI_SCALE, itemY + ActionMenu::ITEM_HEIGHT * 0.5f, label, nullptr);

    if (hasSubmenu) {
        // Right-pointing triangle marks an expandable item.
        const float cx = x + width - 12.0f * UI_SCALE;
        const float cy = itemY + ActionMenu::ITEM_HEIGHT * 0.5f;
        const float s = 3.5f * UI_SCALE;
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx - s, cy - s);
        nvgLineTo(vg, cx + s, cy);
        nvgLineTo(vg, cx - s, cy + s);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGB(200, 200, 205));
        nvgFill(vg);
    }

    if (separatorBelow) {
        const float lineY = itemY + ActionMenu::ITEM_HEIGHT + ActionMenu::SEPARATOR_HEIGHT * 0.5f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + 8.0f * UI_SCALE, lineY);
        nvgLineTo(vg, x + width - 8.0f * UI_SCALE, lineY);
        nvgStrokeColor(vg, nvgRGB(60, 60, 66));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }
}

void DrawActionMenu(NVGcontext* vg, const ActionMenu& menu)
{
    if (!menu.IsOpen()) {
        return;
    }

    const float x = menu.GetX();
    const float y = menu.GetY();
    const float width = ActionMenu::WIDTH;

    DrawPanelBackground(vg, x, y, width, menu.GetPanelHeight());

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, MENU_FONT_SIZE);

    const std::vector<std::string>& items = menu.GetItems();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const float itemY = y + ActionMenu::PADDING + menu.ItemOffsetY(i);
        // The parent of the open submenu stays highlighted.
        const bool hovered = (i == menu.GetHoveredIndex()) || (i == menu.GetOpenSubmenu());
        DrawItem(vg, x, itemY, width, items[static_cast<std::size_t>(i)].c_str(), hovered,
                 menu.ItemHasSubmenu(i), menu.IsSeparatorAfter(i));
    }

    if (menu.GetOpenSubmenu() >= 0) {
        const float sx = menu.GetSubmenuX();
        const float sy = menu.GetSubmenuY();
        DrawPanelBackground(vg, sx, sy, width, menu.GetSubmenuPanelHeight());
        const std::vector<std::string>& subItems = menu.GetSubmenuItems();
        for (int i = 0; i < static_cast<int>(subItems.size()); ++i) {
            const float itemY = sy + ActionMenu::PADDING + menu.SubmenuItemOffsetY(i);
            DrawItem(vg, sx, itemY, width, subItems[static_cast<std::size_t>(i)].c_str(),
                     i == menu.GetSubmenuHoveredIndex(), false,
                     menu.IsSubmenuSeparatorAfter(i));
        }
    }
}
