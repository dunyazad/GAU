#include "core/TypeRegistry.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "render/GraphLayout.h"
#include "render/RenderCanvas.h"

#include <cmath>
#include <cstdio>

using namespace gau;

static int failCount = 0;

static void CheckNear(float actual, float expected, const char* label)
{
    if (std::fabs(actual - expected) > 0.01f) {
        std::printf("FAIL: %s: expected %.3f, got %.3f\n", label, expected, actual);
        ++failCount;
    }
}

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static void TestCanvas()
{
    render::Canvas canvas;
    canvas.PanByScreenDelta(-30.0f, 50.0f);
    canvas.ZoomAt(render::Vec2{200.0f, 120.0f}, 1.5f);

    // Round trip.
    const render::Vec2 c{123.0f, -45.0f};
    const render::Vec2 back = canvas.ScreenToCanvas(canvas.CanvasToScreen(c));
    CheckNear(back.x, c.x, "canvas round trip x");
    CheckNear(back.y, c.y, "canvas round trip y");

    // Zoom keeps the cursor point fixed.
    const render::Vec2 fixedBefore = canvas.ScreenToCanvas(render::Vec2{200.0f, 120.0f});
    canvas.ZoomAt(render::Vec2{200.0f, 120.0f}, 1.3f);
    const render::Vec2 fixedAfter = canvas.ScreenToCanvas(render::Vec2{200.0f, 120.0f});
    CheckNear(fixedAfter.x, fixedBefore.x, "cursor-fixed zoom x");
    CheckNear(fixedAfter.y, fixedBefore.y, "cursor-fixed zoom y");
}

static void TestLayout()
{
    TypeRegistry types;
    const TypeId i = types.Builtin(TypeTag::Int);
    NodeClassRegistry classes;
    NodeClass add;
    add.name = "Add";
    add.category = "Pure";
    add.pins = {{PinDirection::Input, i, "A"}, {PinDirection::Output, i, "R"}};
    classes.Register(add);

    Graph g(types);
    const NodeId n = g.AddNode(*classes.Find("Add"), 10.0f, 20.0f);

    const render::MeasureTextFn measure = [](const std::string& s, float size) {
        return static_cast<float>(s.size()) * size * 0.5f;
    };
    const render::GraphLayout layout = render::ComputeGraphLayout(g, classes, measure);

    const render::NodeLayout* nl = layout.FindNode(n);
    Check(nl != nullptr, "node layout found");
    CheckNear(nl->w, 160.0f, "node min width");
    CheckNear(nl->h, 64.0f, "node height 1 row");

    const PinId inPin = g.FindNode(n)->inputs[0].id;
    const PinId outPin = g.FindNode(n)->outputs[0].id;
    const render::PinLayout* pin = layout.FindPin(inPin);
    const render::PinLayout* out = layout.FindPin(outPin);
    Check(pin != nullptr && out != nullptr, "pin layouts found");
    CheckNear(pin->x, 22.0f, "input pin left");
    CheckNear(out->x, 158.0f, "output pin right");
    CheckNear(pin->y, 65.0f, "pin row y");
}

int main()
{
    TestCanvas();
    TestLayout();
    if (failCount == 0) {
        std::printf("render_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
