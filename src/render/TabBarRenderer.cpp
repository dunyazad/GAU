#include "TabBarRenderer.h"

#include "interaction/TabBar.h"

#include <nanovg.h>

static const char* FONT_REGULAR = "sans";
static const float TAB_FONT_SIZE = 13.0f * UI_SCALE;

static void DrawBarButton(NVGcontext* vg, const UIRect& rect, const char* label)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(45, 45, 50));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, TAB_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(230, 230, 235));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, label, nullptr);
}

void DrawTabBar(NVGcontext* vg, const std::vector<std::string>& tabNames,
                int activeTabIndex, float screenWidth)
{
    // Bar background.
    nvgBeginPath(vg);
    nvgRect(vg, 0.0f, 0.0f, screenWidth, TAB_BAR_HEIGHT);
    nvgFillColor(vg, nvgRGB(18, 18, 20));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0.0f, TAB_BAR_HEIGHT - 0.5f);
    nvgLineTo(vg, screenWidth, TAB_BAR_HEIGHT - 0.5f);
    nvgStrokeColor(vg, nvgRGB(50, 50, 56));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, TAB_FONT_SIZE);

    for (int i = 0; i < static_cast<int>(tabNames.size()); ++i) {
        const UIRect tab = TabRect(i);
        const bool active = (i == activeTabIndex);

        nvgBeginPath(vg);
        nvgRect(vg, tab.x, tab.y, tab.w, tab.h);
        nvgFillColor(vg, active ? nvgRGB(34, 34, 40) : nvgRGB(24, 24, 27));
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGB(50, 50, 56));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        if (active) {
            nvgBeginPath(vg);
            nvgRect(vg, tab.x, tab.y, tab.w, 2.0f * UI_SCALE);
            nvgFillColor(vg, nvgRGB(70, 110, 180));
            nvgFill(vg);
        }

        // Name, clipped to leave room for the close button.
        nvgSave(vg);
        nvgIntersectScissor(vg, tab.x, tab.y, tab.w - TAB_CLOSE_WIDTH, tab.h);
        nvgFillColor(vg, active ? nvgRGB(235, 235, 240) : nvgRGB(160, 160, 168));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, tab.x + 8.0f * UI_SCALE, tab.y + tab.h * 0.5f,
                tabNames[static_cast<std::size_t>(i)].c_str(), nullptr);
        nvgRestore(vg);

        const UIRect close = TabCloseRect(i);
        nvgFillColor(vg, nvgRGB(140, 140, 148));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, close.x + close.w * 0.5f, close.y + close.h * 0.5f, "x", nullptr);
    }

    const UIRect newTab = NewTabRect(static_cast<int>(tabNames.size()));
    nvgFillColor(vg, nvgRGB(170, 170, 178));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, newTab.x + newTab.w * 0.5f, newTab.y + newTab.h * 0.5f, "+", nullptr);

    DrawBarButton(vg, OpenButtonRect(screenWidth), "Open");
    DrawBarButton(vg, SaveButtonRect(screenWidth), "Save");
    DrawBarButton(vg, SaveAsButtonRect(screenWidth), "Save As");
}
