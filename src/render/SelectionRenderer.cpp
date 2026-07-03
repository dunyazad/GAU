#include "SelectionRenderer.h"
#include "Canvas.h"

#include <nanovg.h>

void DrawRubberBand(NVGcontext* vg, const Canvas& canvas,
                    float canvasX0, float canvasY0, float canvasX1, float canvasY1)
{
    const Vec2 a = canvas.CanvasToScreen(Vec2{canvasX0, canvasY0});
    const Vec2 b = canvas.CanvasToScreen(Vec2{canvasX1, canvasY1});

    const float left = (a.x < b.x) ? a.x : b.x;
    const float top = (a.y < b.y) ? a.y : b.y;
    const float width = (a.x < b.x) ? (b.x - a.x) : (a.x - b.x);
    const float height = (a.y < b.y) ? (b.y - a.y) : (a.y - b.y);

    nvgBeginPath(vg);
    nvgRect(vg, left, top, width, height);
    nvgFillColor(vg, nvgRGBA(70, 110, 180, 40));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(70, 110, 180, 200));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}
