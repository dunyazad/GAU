// v2 canvas transform.

#include "RenderCanvas.h"

namespace gau::render {

static const float MIN_ZOOM = 0.1f;
static const float MAX_ZOOM = 4.0f;

Vec2 Canvas::CanvasToScreen(Vec2 canvasPoint) const
{
    return Vec2{canvasPoint.x * zoom + pan.x, canvasPoint.y * zoom + pan.y};
}

Vec2 Canvas::ScreenToCanvas(Vec2 screenPoint) const
{
    return Vec2{(screenPoint.x - pan.x) / zoom, (screenPoint.y - pan.y) / zoom};
}

void Canvas::PanByScreenDelta(float dx, float dy)
{
    pan.x += dx;
    pan.y += dy;
}

void Canvas::ZoomAt(Vec2 screenPoint, float factor)
{
    const Vec2 canvasBefore = ScreenToCanvas(screenPoint);
    zoom *= factor;
    if (zoom < MIN_ZOOM) {
        zoom = MIN_ZOOM;
    }
    if (zoom > MAX_ZOOM) {
        zoom = MAX_ZOOM;
    }
    pan.x = screenPoint.x - canvasBefore.x * zoom;
    pan.y = screenPoint.y - canvasBefore.y * zoom;
}

} // namespace gau::render
