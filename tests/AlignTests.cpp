#include "interaction/Align.h"

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
    return std::fabs(a - b) < 0.001f;
}

static std::vector<NodeBox> Boxes()
{
    // Three boxes at varying x/y, width 100 / height 40.
    return {
        NodeBox{1, 0.0f, 0.0f, 100.0f, 40.0f},
        NodeBox{2, 50.0f, 100.0f, 100.0f, 40.0f},
        NodeBox{3, 200.0f, 300.0f, 100.0f, 40.0f},
    };
}

static void TestAlignLeft()
{
    const auto r = ComputeAlign(Boxes(), AlignMode::Left);
    Check(Near(r[0].x, 0.0f) && Near(r[1].x, 0.0f) && Near(r[2].x, 0.0f), "left aligns x to min");
    Check(Near(r[1].y, 100.0f), "left leaves y unchanged");
}

static void TestAlignRight()
{
    const auto r = ComputeAlign(Boxes(), AlignMode::Right);
    // max right edge = 200 + 100 = 300; each x = 300 - 100 = 200.
    Check(Near(r[0].x, 200.0f) && Near(r[1].x, 200.0f) && Near(r[2].x, 200.0f),
          "right aligns right edges");
}

static void TestAlignBottom()
{
    const auto r = ComputeAlign(Boxes(), AlignMode::Bottom);
    // max bottom = 300 + 40 = 340; each y = 340 - 40 = 300.
    Check(Near(r[0].y, 300.0f) && Near(r[1].y, 300.0f) && Near(r[2].y, 300.0f),
          "bottom aligns bottom edges");
}

static void TestAlignCenterHorizontal()
{
    const auto r = ComputeAlign(Boxes(), AlignMode::CenterHorizontal);
    // centers: 50, 100, 250 -> avg 133.333; x = avg - 50.
    Check(Near(r[0].x, 83.333f) && Near(r[1].x, 83.333f) && Near(r[2].x, 83.333f),
          "center-h aligns vertical center lines");
}

static void TestDistributeHorizontal()
{
    // Centers at x: 50 (id1), 150 (id2), 850 (id3). Distribute evenly ->
    // middle center at (50 + 850) / 2 = 450; x = 450 - 50 = 400.
    std::vector<NodeBox> boxes = {
        NodeBox{1, 0.0f, 0.0f, 100.0f, 40.0f},
        NodeBox{2, 100.0f, 0.0f, 100.0f, 40.0f},
        NodeBox{3, 800.0f, 0.0f, 100.0f, 40.0f},
    };
    const auto r = ComputeDistribute(boxes, true);
    Check(Near(r[0].x, 0.0f) && Near(r[2].x, 800.0f), "distribute keeps ends fixed");
    Check(Near(r[1].x, 400.0f), "distribute evenly spaces the middle");
}

static void TestSmallSetsUnchanged()
{
    std::vector<NodeBox> one = {NodeBox{1, 5.0f, 6.0f, 10.0f, 10.0f}};
    Check(ComputeAlign(one, AlignMode::Left)[0].x == 5.0f, "single box unchanged by align");
    std::vector<NodeBox> two = {NodeBox{1, 0, 0, 10, 10}, NodeBox{2, 100, 0, 10, 10}};
    const auto d = ComputeDistribute(two, true);
    Check(Near(d[0].x, 0.0f) && Near(d[1].x, 100.0f), "two boxes unchanged by distribute");
}

// A -> B -> C chain arranges into three columns left to right; an
// unlinked node shares the first column without overlapping.
static void TestAutoLayoutLayers()
{
    std::vector<NodeBox> boxes = {NodeBox{1, 300.0f, 0.0f, 100.0f, 50.0f},
                                  NodeBox{2, 0.0f, 200.0f, 100.0f, 50.0f},
                                  NodeBox{3, 150.0f, 100.0f, 100.0f, 50.0f},
                                  NodeBox{4, 500.0f, 500.0f, 100.0f, 50.0f}};
    // 1 -> 3 -> 2; 4 unlinked.
    const std::vector<LayoutEdge> edges = {LayoutEdge{0, 2}, LayoutEdge{2, 1}};
    const auto out = ComputeAutoLayout(boxes, edges);
    Check(out.size() == 4, "auto layout returns every box");
    // Origin is the set's top-left corner (x from box 2, y from box 1).
    Check(Near(out[0].x, 0.0f), "layer 0 sits at the origin column");
    Check(out[2].x > out[0].x, "successor moves one column right");
    Check(out[1].x > out[2].x, "second successor moves further right");
    Check(Near(out[3].x, out[0].x), "unlinked node shares the first column");
    Check(!Near(out[3].y, out[0].y), "stacked nodes in a column do not overlap");
}

int main()
{
    TestAlignLeft();
    TestAlignRight();
    TestAlignBottom();
    TestAlignCenterHorizontal();
    TestDistributeHorizontal();
    TestSmallSetsUnchanged();
    TestAutoLayoutLayers();
    if (failCount == 0) {
        std::printf("align_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
