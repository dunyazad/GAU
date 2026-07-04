#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/Runtime.h"
#include "exec/StructNodes.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include <cstdio>
#include <variant>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static void TestMakeBreakRoundTrip()
{
    TypeRegistry types;
    const TypeId i = types.Builtin(TypeTag::Int);
    StructDef point;
    point.name = "Point";
    point.fields = {{"x", i}, {"y", i}};
    types.DefineStruct(point);

    NodeClassRegistry classes;
    NodeClass makeInt;
    makeInt.name = "MakeInt";
    makeInt.category = "Pure";
    makeInt.pins = {{PinDirection::Output, i, "Value"}};
    makeInt.properties = {{"Value", i, Value::Int(0)}};
    classes.Register(makeInt);

    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    RegisterStructNodes(classes, builtins, types, point);

    Check(classes.Find("Make Point") != nullptr, "Make Point class registered");
    Check(classes.Find("Break Point") != nullptr, "Break Point class registered");

    Graph g(types);
    const NodeId mx = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId my = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId mk = g.AddNode(*classes.Find("Make Point"), 0, 0);
    const NodeId bk = g.AddNode(*classes.Find("Break Point"), 0, 0);
    g.FindNode(mx)->properties[0] = Value::Int(2);
    g.FindNode(my)->properties[0] = Value::Int(3);

    g.AddLink(g.FindNode(mx)->outputs[0].id, g.FindNode(mk)->inputs[0].id);
    g.AddLink(g.FindNode(my)->outputs[0].id, g.FindNode(mk)->inputs[1].id);
    g.AddLink(g.FindNode(mk)->outputs[0].id, g.FindNode(bk)->inputs[0].id);

    Runtime rt(g, types, classes, builtins, [](const std::string&) {});
    const PinId breakX = g.FindNode(bk)->outputs[0].id;
    const PinId breakY = g.FindNode(bk)->outputs[1].id;
    Check(rt.EvalPin(breakX) == Value::Int(2), "Break Point x == 2");
    Check(rt.EvalPin(breakY) == Value::Int(3), "Break Point y == 3");
}

int main()
{
    TestMakeBreakRoundTrip();
    if (failCount == 0) {
        std::printf("struct_nodes_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
