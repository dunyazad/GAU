#include "LogPanelRenderer.h"

#include "interaction/LogPanel.h"

#include <nanovg.h>

static const char* FONT_REGULAR = "sans";
static const float LOG_FONT_SIZE = 12.0f * UI_SCALE;

void DrawLogPanel(NVGcontext* vg, const LogPanel& panel,
                  const std::vector<std::string>& lines,
                  float screenWidth, float screenHeight)
{
    const UIRect header = panel.HeaderRect(screenWidth, screenHeight);

    // Header strip.
    nvgBeginPath(vg);
    nvgRect(vg, header.x, header.y, header.w, header.h);
    nvgFillColor(vg, nvgRGB(24, 24, 28));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0.0f, header.y + 0.5f);
    nvgLineTo(vg, screenWidth, header.y + 0.5f);
    nvgStrokeColor(vg, nvgRGB(50, 50, 56));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, LOG_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(180, 180, 188));
    const std::string title = std::string(panel.IsCollapsed() ? "> " : "v ")
                            + "Output (" + std::to_string(lines.size()) + ")";
    nvgText(vg, header.x + LogPanel::PADDING, header.y + header.h * 0.5f,
            title.c_str(), nullptr);

    // Clear button.
    const UIRect clear = panel.ClearButtonRect(screenWidth, screenHeight);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, clear.x, clear.y, clear.w, clear.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(45, 45, 50));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGB(210, 210, 215));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, clear.x + clear.w * 0.5f, clear.y + clear.h * 0.5f, "Clear", nullptr);

    if (panel.IsCollapsed()) {
        return;
    }

    // Body with the newest lines at the bottom.
    const UIRect body = panel.BodyRect(screenWidth, screenHeight);
    nvgBeginPath(vg);
    nvgRect(vg, body.x, body.y, body.w, body.h);
    nvgFillColor(vg, nvgRGBA(14, 14, 16, 245));
    nvgFill(vg);

    const int visibleCount = panel.VisibleLineCount();
    int firstLine = static_cast<int>(lines.size()) - visibleCount - panel.GetScrollLines();
    if (firstLine < 0) {
        firstLine = 0;
    }

    nvgSave(vg);
    nvgIntersectScissor(vg, body.x, body.y, body.w, body.h);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGB(200, 200, 205));
    for (int i = 0; i < visibleCount; ++i) {
        const int lineIndex = firstLine + i;
        if (lineIndex < 0 || lineIndex >= static_cast<int>(lines.size())) {
            continue;
        }
        const float lineY = body.y + LogPanel::LINE_HEIGHT * (static_cast<float>(i) + 0.5f);
        nvgText(vg, body.x + LogPanel::PADDING, lineY,
                lines[static_cast<std::size_t>(lineIndex)].c_str(), nullptr);
    }
    nvgRestore(vg);
}
