#include "GridRenderer.h"
#include "Canvas.h"

#include <nanovg.h>

#include <cmath>

static const float GRID_MINOR_STEP = 16.0f;
static const float GRID_MAJOR_STEP = GRID_MINOR_STEP * 8.0f;

// Minor lines are skipped when their on-screen spacing gets too dense
// to read (far zoom-out); the major grid stays visible.
static const float MIN_VISIBLE_STEP_PX = 4.0f;

static void DrawGridLevel(NVGcontext* vg, const Canvas& canvas,
                          float screenWidth, float screenHeight,
                          float stepCanvas, NVGcolor color)
{
    const float stepScreen = stepCanvas * canvas.GetZoom();
    if (stepScreen < MIN_VISIBLE_STEP_PX) {
        return;
    }

    const Vec2 topLeft = canvas.ScreenToCanvas(Vec2{0.0f, 0.0f});
    const Vec2 bottomRight = canvas.ScreenToCanvas(Vec2{screenWidth, screenHeight});

    nvgBeginPath(vg);

    const float firstX = std::floor(topLeft.x / stepCanvas) * stepCanvas;
    for (float x = firstX; x <= bottomRight.x; x += stepCanvas) {
        const float screenX = canvas.CanvasToScreen(Vec2{x, 0.0f}).x;
        nvgMoveTo(vg, screenX, 0.0f);
        nvgLineTo(vg, screenX, screenHeight);
    }

    const float firstY = std::floor(topLeft.y / stepCanvas) * stepCanvas;
    for (float y = firstY; y <= bottomRight.y; y += stepCanvas) {
        const float screenY = canvas.CanvasToScreen(Vec2{0.0f, y}).y;
        nvgMoveTo(vg, 0.0f, screenY);
        nvgLineTo(vg, screenWidth, screenY);
    }

    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}

void DrawGrid(NVGcontext* vg, const Canvas& canvas, float screenWidth, float screenHeight)
{
    const NVGcolor minorColor = nvgRGB(41, 41, 43);
    const NVGcolor majorColor = nvgRGB(26, 26, 28);

    DrawGridLevel(vg, canvas, screenWidth, screenHeight, GRID_MINOR_STEP, minorColor);
    DrawGridLevel(vg, canvas, screenWidth, screenHeight, GRID_MAJOR_STEP, majorColor);
}
