#include "core/TypeRegistry.h"
#include "interaction/HitTest2.h"
#include "interaction/InteractionFsm.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "render/GraphLayout.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static float FakeMeasure(const std::string& s, float size)
{
    return static_cast<float>(s.size()) * size * 0.5f;
}

struct Fixture
{
    TypeRegistry types;
    NodeClassRegistry classes;
    Graph graph{types};
    NodeId makeInt = INVALID_ID;
    NodeId print = INVALID_ID;

    Fixture()
    {
        const TypeId exec = types.Builtin(TypeTag::Exec);
        const TypeId i = types.Builtin(TypeTag::Int);
        NodeClass mk;
        mk.name = "MakeInt";
        mk.pins = {{PinDirection::Output, i, "Value"}};
        classes.Register(mk);
        NodeClass pr;
        pr.name = "PrintInt";
        pr.pins = {{PinDirection::Input, exec, "Exec"},
                   {PinDirection::Input, i, "Value"},
                   {PinDirection::Output, exec, "Then"}};
        classes.Register(pr);
        makeInt = graph.AddNode(*classes.Find("MakeInt"), 0.0f, 0.0f);
        print = graph.AddNode(*classes.Find("PrintInt"), 300.0f, 0.0f);
    }

    render::GraphLayout Layout()
    {
        return render::ComputeGraphLayout(graph, classes, FakeMeasure);
    }
};

static void TestNodeDrag()
{
    Fixture f;
    render::GraphLayout layout = f.Layout();
    InteractionFsm fsm;
    fsm.OnMouseDown(20.0f, 20.0f, f.graph, layout); // inside MakeInt body
    Check(fsm.GetState() == InteractionFsm::State::DraggingNodes, "dragging node");
    Check(fsm.IsSelected(f.makeInt), "MakeInt selected");
    fsm.OnMouseMove(50.0f, 60.0f, f.graph, layout);  // +30, +40
    fsm.OnMouseUp(50.0f, 60.0f, f.graph, layout);
    const Node* n = f.graph.FindNode(f.makeInt);
    Check(std::fabs(n->x - 30.0f) < 0.01f && std::fabs(n->y - 40.0f) < 0.01f, "node moved by delta");
}

static void TestLinkCreate()
{
    Fixture f;
    render::GraphLayout layout = f.Layout();
    // MakeInt output pin ~ (148, 45); PrintInt Value input ~ (312, 67).
    InteractionFsm fsm;
    fsm.OnMouseDown(148.0f, 45.0f, f.graph, layout);
    Check(fsm.IsDraggingLink(), "dragging link from output pin");
    fsm.OnMouseMove(312.0f, 67.0f, f.graph, layout);
    fsm.OnMouseUp(312.0f, 67.0f, f.graph, layout);
    Check(f.graph.Links().size() == 1, "link created between int pins");
}

static void TestRubberBand()
{
    Fixture f;
    render::GraphLayout layout = f.Layout();
    InteractionFsm fsm;
    fsm.OnMouseDown(600.0f, 300.0f, f.graph, layout); // empty
    Check(fsm.IsRubberBanding(), "rubber banding");
    fsm.OnMouseMove(-10.0f, -10.0f, f.graph, layout); // sweep over both nodes
    Check(fsm.Selection().size() == 2, "both nodes selected");
    fsm.OnMouseUp(-10.0f, -10.0f, f.graph, layout);
    Check(fsm.GetState() == InteractionFsm::State::Idle, "idle after release");
}

// A point near the middle of a link's curve hits it; a far point does
// not.
static void TestLinkHit()
{
    Fixture f;
    const PinId out = f.graph.FindNode(f.makeInt)->outputs[0].id;
    const PinId in = f.graph.FindNode(f.print)->inputs[1].id;
    f.graph.AddLink(out, in);
    const render::GraphLayout layout = f.Layout();

    const render::PinLayout* fromPin = layout.FindPin(out);
    const render::PinLayout* toPin = layout.FindPin(in);
    Check(fromPin != nullptr && toPin != nullptr, "link pin layout exists");
    const float midX = (fromPin->x + toPin->x) * 0.5f;
    const float midY = (fromPin->y + toPin->y) * 0.5f;
    Check(HitTestLink(f.graph, layout, midX, midY, 12.0f) != INVALID_ID,
          "midpoint hits the link curve");
    Check(HitTestLink(f.graph, layout, midX, midY - 300.0f, 12.0f) == INVALID_ID,
          "far point misses the link");
}

int main()
{
    TestNodeDrag();
    TestLinkCreate();
    TestRubberBand();
    TestLinkHit();
    if (failCount == 0) {
        std::printf("interaction_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
