#include "Canvas.h"

static float ClampZoom(float value)
{
    if (value < Canvas::MIN_ZOOM) {
        return Canvas::MIN_ZOOM;
    }
    if (value > Canvas::MAX_ZOOM) {
        return Canvas::MAX_ZOOM;
    }
    return value;
}

Vec2 Canvas::CanvasToScreen(Vec2 canvasPoint) const
{
    Vec2 result;
    result.x = (canvasPoint.x - panX) * zoom;
    result.y = (canvasPoint.y - panY) * zoom;
    return result;
}

Vec2 Canvas::ScreenToCanvas(Vec2 screenPoint) const
{
    Vec2 result;
    result.x = screenPoint.x / zoom + panX;
    result.y = screenPoint.y / zoom + panY;
    return result;
}

void Canvas::PanByScreenDelta(float dxScreen, float dyScreen)
{
    panX -= dxScreen / zoom;
    panY -= dyScreen / zoom;
}

void Canvas::ZoomAt(Vec2 screenPoint, float zoomFactor)
{
    const Vec2 canvasUnderCursor = ScreenToCanvas(screenPoint);
    zoom = ClampZoom(zoom * zoomFactor);
    panX = canvasUnderCursor.x - screenPoint.x / zoom;
    panY = canvasUnderCursor.y - screenPoint.y / zoom;
}
