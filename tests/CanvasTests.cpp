#include "render/Canvas.h"

#include <cmath>
#include <cstdio>

static int failCount = 0;

static void CheckNear(float actual, float expected, const char* label)
{
    const float tolerance = 0.0005f;
    if (std::fabs(actual - expected) > tolerance) {
        std::printf("FAIL: %s: expected %.6f, got %.6f\n", label, expected, actual);
        ++failCount;
    }
}

static void TestIdentityTransform()
{
    Canvas canvas;
    const Vec2 screen = canvas.CanvasToScreen(Vec2{37.5f, -12.25f});
    CheckNear(screen.x, 37.5f, "identity canvasToScreen x");
    CheckNear(screen.y, -12.25f, "identity canvasToScreen y");
}

static void TestRoundTrip()
{
    Canvas canvas;
    canvas.PanByScreenDelta(-123.0f, 456.0f);
    canvas.ZoomAt(Vec2{200.0f, 150.0f}, 1.7f);

    const Vec2 canvasPoints[] = {
        Vec2{0.0f, 0.0f},
        Vec2{100.0f, 200.0f},
        Vec2{-350.75f, 42.5f},
        Vec2{10000.0f, -9876.5f},
    };
    for (const Vec2& point : canvasPoints) {
        const Vec2 roundTrip = canvas.ScreenToCanvas(canvas.CanvasToScreen(point));
        CheckNear(roundTrip.x, point.x, "roundTrip x");
        CheckNear(roundTrip.y, point.y, "roundTrip y");
    }
}

static void TestPanByScreenDelta()
{
    Canvas canvas;
    canvas.ZoomAt(Vec2{0.0f, 0.0f}, 2.0f);

    const Vec2 canvasPoint{50.0f, 60.0f};
    const Vec2 before = canvas.CanvasToScreen(canvasPoint);
    canvas.PanByScreenDelta(30.0f, -20.0f);
    const Vec2 after = canvas.CanvasToScreen(canvasPoint);

    CheckNear(after.x - before.x, 30.0f, "pan screen delta x");
    CheckNear(after.y - before.y, -20.0f, "pan screen delta y");
}

static void TestZoomAtKeepsCursorFixed()
{
    Canvas canvas;
    canvas.PanByScreenDelta(77.0f, -33.0f);

    const Vec2 cursor{640.0f, 360.0f};
    const Vec2 canvasUnderCursor = canvas.ScreenToCanvas(cursor);

    canvas.ZoomAt(cursor, 1.25f);
    const Vec2 afterZoomIn = canvas.CanvasToScreen(canvasUnderCursor);
    CheckNear(afterZoomIn.x, cursor.x, "zoomAt fixed x (in)");
    CheckNear(afterZoomIn.y, cursor.y, "zoomAt fixed y (in)");

    canvas.ZoomAt(cursor, 0.4f);
    const Vec2 afterZoomOut = canvas.CanvasToScreen(canvasUnderCursor);
    CheckNear(afterZoomOut.x, cursor.x, "zoomAt fixed x (out)");
    CheckNear(afterZoomOut.y, cursor.y, "zoomAt fixed y (out)");
}

static void TestZoomClamp()
{
    Canvas canvas;

    canvas.ZoomAt(Vec2{100.0f, 100.0f}, 1000.0f);
    CheckNear(canvas.GetZoom(), Canvas::MAX_ZOOM, "zoom clamp max");

    canvas.ZoomAt(Vec2{100.0f, 100.0f}, 0.000001f);
    CheckNear(canvas.GetZoom(), Canvas::MIN_ZOOM, "zoom clamp min");

    // Cursor anchoring must hold even when the zoom value clamps.
    const Vec2 cursor{320.0f, 240.0f};
    const Vec2 canvasUnderCursor = canvas.ScreenToCanvas(cursor);
    canvas.ZoomAt(cursor, 0.5f);
    const Vec2 after = canvas.CanvasToScreen(canvasUnderCursor);
    CheckNear(after.x, cursor.x, "zoom clamp fixed x");
    CheckNear(after.y, cursor.y, "zoom clamp fixed y");
}

int main()
{
    TestIdentityTransform();
    TestRoundTrip();
    TestPanByScreenDelta();
    TestZoomAtKeepsCursorFixed();
    TestZoomClamp();

    if (failCount == 0) {
        std::printf("CanvasTests: all tests passed\n");
        return 0;
    }
    std::printf("CanvasTests: %d check(s) failed\n", failCount);
    return 1;
}
