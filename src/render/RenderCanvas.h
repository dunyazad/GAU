#pragma once

// v2 canvas transform: pan/zoom between canvas and screen space. Pure
// math, unit-testable. (gau::render, distinct from the v1 Canvas.)

namespace gau::render {

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

class Canvas
{
public:
    Vec2 CanvasToScreen(Vec2 canvasPoint) const;
    Vec2 ScreenToCanvas(Vec2 screenPoint) const;

    void PanByScreenDelta(float dx, float dy);
    // Zooms by factor while keeping the screen point fixed (cursor zoom).
    void ZoomAt(Vec2 screenPoint, float factor);

    float Zoom() const { return zoom; }
    Vec2 Pan() const { return pan; }

private:
    Vec2 pan{0.0f, 0.0f};
    float zoom = 1.0f;
};

} // namespace gau::render
