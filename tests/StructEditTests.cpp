#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/Runtime.h"
#include "exec/StructEdit.h"
#include "exec/StructNodes.h"
#include "model/Project.h"

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

static void RegisterMakeInt(NodeClassRegistry& classes, const TypeRegistry& t)
{
    const TypeId i = t.Builtin(TypeTag::Int);
    NodeClass makeInt;
    makeInt.name = "MakeInt";
    makeInt.category = "Pure";
    makeInt.pins = {{PinDirection::Output, i, "Value"}};
    makeInt.properties = {{"Value", i, Value::Int(0)}};
    classes.Register(makeInt);
}

// Adds a z field to Point{x,y}; existing Make/Break instances grow to match and
// the new field flows through end to end while the originals still read back.
static void TestAddField()
{
    Project p;
    const TypeId i = p.types.Builtin(TypeTag::Int);
    StructDef point;
    point.name = "Point";
    point.fields = {{"x", i}, {"y", i}};
    p.types.DefineStruct(point);

    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    RegisterMakeInt(p.classes, p.types);
    RegisterStructNodes(p.classes, builtins, p.types, point);

    Graph& g = *p.graph;
    const NodeId mx = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId my = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId mk = g.AddNode(*p.classes.Find("Make Point"), 0, 0);
    const NodeId bk = g.AddNode(*p.classes.Find("Break Point"), 0, 0);
    g.FindNode(mx)->properties[0] = Value::Int(2);
    g.FindNode(my)->properties[0] = Value::Int(3);
    g.AddLink(g.FindNode(mx)->outputs[0].id, g.FindNode(mk)->inputs[0].id);
    g.AddLink(g.FindNode(my)->outputs[0].id, g.FindNode(mk)->inputs[1].id);
    g.AddLink(g.FindNode(mk)->outputs[0].id, g.FindNode(bk)->inputs[0].id);

    AddStructField(p.types, "Point", "z", i, p.classes, builtins, p);
    Check(p.types.FindStruct("Point")->fields.size() == 3, "Point has three fields");
    Check(g.FindNode(mk)->inputs.size() == 3, "Make Point grew to three inputs");
    Check(g.FindNode(bk)->outputs.size() == 3, "Break Point grew to three outputs");

    // Feed z = 7 on the new input, read it back through the new Break output.
    const NodeId mz = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    g.FindNode(mz)->properties[0] = Value::Int(7);
    g.AddLink(g.FindNode(mz)->outputs[0].id, g.FindNode(mk)->inputs[2].id);

    Runtime rt(g, p.types, p.classes, builtins, [](const std::string&) {});
    Check(rt.EvalPin(g.FindNode(bk)->outputs[0].id) == Value::Int(2), "x still reads 2");
    Check(rt.EvalPin(g.FindNode(bk)->outputs[2].id) == Value::Int(7), "new z reads 7");
}

// Removing the last field shrinks instances and drops that pin's links.
static void TestRemoveField()
{
    Project p;
    const TypeId i = p.types.Builtin(TypeTag::Int);
    StructDef point;
    point.name = "Point";
    point.fields = {{"x", i}, {"y", i}};
    p.types.DefineStruct(point);

    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    RegisterMakeInt(p.classes, p.types);
    RegisterStructNodes(p.classes, builtins, p.types, point);

    Graph& g = *p.graph;
    const NodeId mx = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId my = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId mk = g.AddNode(*p.classes.Find("Make Point"), 0, 0);
    g.FindNode(mx)->properties[0] = Value::Int(2);
    g.FindNode(my)->properties[0] = Value::Int(3);
    g.AddLink(g.FindNode(mx)->outputs[0].id, g.FindNode(mk)->inputs[0].id);
    g.AddLink(g.FindNode(my)->outputs[0].id, g.FindNode(mk)->inputs[1].id);
    Check(g.Links().size() == 2, "two links before removal");

    RemoveStructField(p.types, "Point", 1, p.classes, builtins, p);
    Check(p.types.FindStruct("Point")->fields.size() == 1, "Point has one field");
    Check(g.FindNode(mk)->inputs.size() == 1, "Make Point shrank to one input");
    Check(g.Links().size() == 1, "the y link was dropped with its pin");

    // The surviving field still evaluates.
    const NodeId bk = g.AddNode(*p.classes.Find("Break Point"), 0, 0);
    g.AddLink(g.FindNode(mk)->outputs[0].id, g.FindNode(bk)->inputs[0].id);
    Runtime rt(g, p.types, p.classes, builtins, [](const std::string&) {});
    Check(rt.EvalPin(g.FindNode(bk)->outputs[0].id) == Value::Int(2), "x still reads 2");
}

// Deleting a user type removes its definition; the interned id stays resolvable
// so existing instances do not become dangling.
static void TestDeleteType()
{
    Project p;
    const TypeId i = p.types.Builtin(TypeTag::Int);
    StructDef point;
    point.name = "Point";
    point.fields = {{"x", i}};
    p.types.DefineStruct(point);
    const TypeId pointId = p.types.UserType("Point");

    Check(p.types.RemoveStruct("Point"), "RemoveStruct reports removal");
    Check(p.types.FindStruct("Point") == nullptr, "Point definition gone");
    Check(p.types.Resolve(pointId) != nullptr, "interned Point id still resolves");
    Check(!p.types.RemoveStruct("Point"), "second removal is a no-op");
}

int main()
{
    TestAddField();
    TestRemoveField();
    TestDeleteType();
    if (failCount == 0) {
        std::printf("struct_edit_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
