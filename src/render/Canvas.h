#pragma once

// Simple 2D vector used for canvas/screen coordinates.
struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

// Pan/zoom state and coordinate transforms between screen space
// (logical window pixels) and canvas space (design-spec units at zoom 1).
// Pure math: no rendering or SDL dependencies.
class Canvas
{
public:
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 4.0f;

    float GetZoom() const { return zoom; }
    float GetPanX() const { return panX; }
    float GetPanY() const { return panY; }

    Vec2 CanvasToScreen(Vec2 canvasPoint) const;
    Vec2 ScreenToCanvas(Vec2 screenPoint) const;

    // Moves the view by a screen-space delta (e.g. mouse drag distance).
    void PanByScreenDelta(float dxScreen, float dyScreen);

    // Multiplies zoom by zoomFactor, clamped to [MIN_ZOOM, MAX_ZOOM],
    // keeping the canvas point under screenPoint stationary on screen.
    void ZoomAt(Vec2 screenPoint, float zoomFactor);

    // Restores a persisted view; zoom is clamped to the valid range.
    void SetView(float panXValue, float panYValue, float zoomValue);

private:
    // Canvas coordinates of the screen origin (top-left corner).
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 1.0f;
};
