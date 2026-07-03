#include "ActionMenuRenderer.h"

#include "interaction/ActionMenu.h"

#include <nanovg.h>

static const char* FONT_REGULAR = "sans";
static const float MENU_FONT_SIZE = 13.0f * UI_SCALE;

void DrawActionMenu(NVGcontext* vg, const ActionMenu& menu)
{
    if (!menu.IsOpen()) {
        return;
    }

    const float x = menu.GetX();
    const float y = menu.GetY();
    const float width = ActionMenu::WIDTH;
    const float height = menu.GetPanelHeight();

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 4.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(20, 20, 22, 245));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, MENU_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    const std::vector<std::string>& items = menu.GetItems();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const float itemY = y + ActionMenu::PADDING
                          + ActionMenu::ITEM_HEIGHT * static_cast<float>(i);

        if (i == menu.GetHoveredIndex()) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x + 2.0f * UI_SCALE, itemY, width - 4.0f * UI_SCALE,
                           ActionMenu::ITEM_HEIGHT, 2.0f * UI_SCALE);
            nvgFillColor(vg, nvgRGBA(70, 110, 180, 220));
            nvgFill(vg);
        }

        nvgFillColor(vg, nvgRGB(225, 225, 230));
        nvgText(vg, x + 8.0f * UI_SCALE, itemY + ActionMenu::ITEM_HEIGHT * 0.5f,
                items[static_cast<std::size_t>(i)].c_str(), nullptr);
    }
}
