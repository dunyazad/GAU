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

// The user scenario: a whole struct object connects to a single struct-typed
// input pin of an arbitrary node and is consumed as one value (not as x/y/z
// component pins). Make Vector3f -> SumVec(Vector3f)->float.
static void TestStructPinConnectsToInput()
{
    TypeRegistry types;
    const TypeId f = types.Builtin(TypeTag::Float);
    StructDef vec;
    vec.name = "Vector3f";
    vec.fields = {{"x", f}, {"y", f}, {"z", f}};
    types.DefineStruct(vec);
    const TypeId vecId = types.UserType("Vector3f");

    NodeClassRegistry classes;
    NodeClass makeFloat;
    makeFloat.name = "MakeFloat";
    makeFloat.category = "Pure";
    makeFloat.pins = {{PinDirection::Output, f, "Value"}};
    makeFloat.properties = {{"Value", f, Value::Float(0.0)}};
    classes.Register(makeFloat);

    // A node that takes the whole Vector3f on one input pin and sums it.
    NodeClass sumVec;
    sumVec.name = "SumVec";
    sumVec.category = "Pure";
    sumVec.pins = {{PinDirection::Input, vecId, "V"}, {PinDirection::Output, f, "Sum"}};
    classes.Register(sumVec);

    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    builtins.Register("MakeFloat", [](NodeEval& e) { e.Out(0, e.Prop(0)); });
    builtins.Register("SumVec", [](NodeEval& e) {
        const Value v = e.In(0);
        double sum = 0.0;
        if (const StructVal* sv = std::get_if<StructVal>(&v.data)) {
            for (const Value& field : sv->fields) {
                if (const double* d = std::get_if<double>(&field.data)) {
                    sum += *d;
                }
            }
        }
        e.Out(0, Value::Float(sum));
    });
    RegisterStructNodes(classes, builtins, types, vec);

    Graph g(types);
    const NodeId mx = g.AddNode(*classes.Find("MakeFloat"), 0, 0);
    const NodeId my = g.AddNode(*classes.Find("MakeFloat"), 0, 0);
    const NodeId mz = g.AddNode(*classes.Find("MakeFloat"), 0, 0);
    const NodeId mk = g.AddNode(*classes.Find("Make Vector3f"), 0, 0);
    const NodeId sum = g.AddNode(*classes.Find("SumVec"), 0, 0);
    g.FindNode(mx)->properties[0] = Value::Float(1.0);
    g.FindNode(my)->properties[0] = Value::Float(2.0);
    g.FindNode(mz)->properties[0] = Value::Float(3.0);

    g.AddLink(g.FindNode(mx)->outputs[0].id, g.FindNode(mk)->inputs[0].id);
    g.AddLink(g.FindNode(my)->outputs[0].id, g.FindNode(mk)->inputs[1].id);
    g.AddLink(g.FindNode(mz)->outputs[0].id, g.FindNode(mk)->inputs[2].id);
    // The whole Vector3f flows through one link into SumVec's single input.
    const PinId vecOut = g.FindNode(mk)->outputs[0].id;
    const PinId vecIn = g.FindNode(sum)->inputs[0].id;
    Check(g.CanConnect(vecOut, vecIn), "Vector3f output connects to Vector3f input");
    g.AddLink(vecOut, vecIn);

    Runtime rt(g, types, classes, builtins, [](const std::string&) {});
    Check(rt.EvalPin(g.FindNode(sum)->outputs[0].id) == Value::Float(6.0),
          "struct consumed as one value: 1+2+3 = 6");
}

int main()
{
    TestMakeBreakRoundTrip();
    TestStructPinConnectsToInput();
    if (failCount == 0) {
        std::printf("struct_nodes_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
