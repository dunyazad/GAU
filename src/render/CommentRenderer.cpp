#include "CommentRenderer.h"

#include "model/NodeGraph.h"

#include <nanovg.h>

static const char* FONT_BOLD = "sans-bold";
static const float COMMENT_TITLE_FONT_SIZE = 16.0f;

void DrawComment(NVGcontext* vg, const CommentNode& comment,
                 bool editingTitle, const std::string& editingText)
{
    const float x = comment.x;
    const float y = comment.y;
    const float w = comment.width;
    const float h = comment.height;

    // Translucent body behind the nodes.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 4.0f);
    nvgFillColor(vg, nvgRGBA(100, 120, 150, 45));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(120, 140, 170, 130));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Title bar.
    nvgBeginPath(vg);
    nvgRoundedRectVarying(vg, x, y, w, COMMENT_TITLE_HEIGHT, 4.0f, 4.0f, 0.0f, 0.0f);
    nvgFillColor(vg, nvgRGBA(100, 120, 150, 200));
    nvgFill(vg);
    if (editingTitle) {
        nvgStrokeColor(vg, nvgRGB(70, 110, 180));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }

    // Title text (with caret while editing).
    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, COMMENT_TITLE_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(240, 240, 245));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgSave(vg);
    nvgIntersectScissor(vg, x, y, w, COMMENT_TITLE_HEIGHT);
    const std::string shown = editingTitle ? editingText + "|" : comment.title;
    nvgText(vg, x + 8.0f, y + COMMENT_TITLE_HEIGHT * 0.5f, shown.c_str(), nullptr);
    nvgRestore(vg);

    // Bottom-right resize handle: three diagonal grip lines.
    const float right = x + w;
    const float bottom = y + h;
    nvgBeginPath(vg);
    for (int i = 1; i <= 3; ++i) {
        const float offset = 4.0f * static_cast<float>(i);
        nvgMoveTo(vg, right - offset, bottom - 2.0f);
        nvgLineTo(vg, right - 2.0f, bottom - offset);
    }
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 110));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);
}
