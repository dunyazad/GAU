#include "interaction/Minimap.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.01f;
}

static void TestMinimapFit()
{
    // Content 400x200 at origin; panel 200x200 at (10,10). Aspect-fit scale
    // = min(200/400, 200/200) = 0.5. Scaled content 200x100 centered in the
    // 200x200 panel: x offset 0, y offset 50.
    Bounds content{0.0f, 0.0f, 400.0f, 200.0f};
    ViewRect panel{10.0f, 10.0f, 200.0f, 200.0f};
    Bounds visible{0.0f, 0.0f, 100.0f, 100.0f};
    const MinimapFit fit = ComputeMinimap(content, panel, visible);
    Check(Near(fit.scale, 0.5f), "aspect-fit scale 0.5");
    // world (0,0) -> panel (10, 10 + 50).
    Check(Near(fit.offsetX, 10.0f) && Near(fit.offsetY, 60.0f), "content centered offset");
    // viewport 0..100 -> width 50, at panel origin.
    Check(Near(fit.viewport.x, 10.0f) && Near(fit.viewport.w, 50.0f), "viewport mapped");
}

static void TestNodesInRect()
{
    std::vector<NodeBox> boxes = {
        NodeBox{1, 10.0f, 10.0f, 40.0f, 20.0f},   // fully inside
        NodeBox{2, 90.0f, 90.0f, 40.0f, 20.0f},   // straddles the edge
        NodeBox{3, 200.0f, 200.0f, 10.0f, 10.0f}, // outside
    };
    ViewRect rect{0.0f, 0.0f, 100.0f, 100.0f};
    const auto inside = NodesInRect(boxes, rect);
    Check(inside.size() == 1 && inside[0] == 1, "only fully-contained node grouped");
}

int main()
{
    TestMinimapFit();
    TestNodesInRect();
    if (failCount == 0) {
        std::printf("minimap_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
