#include "core/TypeRegistry.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include <cstdio>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static NodeClass MakeClass(std::string name, std::string category, std::vector<PinDef> pins,
                           std::vector<PropertyDef> props = {})
{
    NodeClass c;
    c.name = std::move(name);
    c.category = std::move(category);
    c.pins = std::move(pins);
    c.properties = std::move(props);
    return c;
}

static void TestConnectionsAndSpawn()
{
    TypeRegistry types;
    const TypeId exec = types.Builtin(TypeTag::Exec);
    const TypeId i = types.Builtin(TypeTag::Int);
    const TypeId s = types.Builtin(TypeTag::String);

    NodeClassRegistry classes;
    classes.Register(MakeClass("MakeInt", "Pure", {{PinDirection::Output, i, "Value"}},
                               {{"Value", i, Value::Int(7)}}));
    classes.Register(MakeClass("AddInt", "Pure",
                               {{PinDirection::Input, i, "A"},
                                {PinDirection::Input, i, "B"},
                                {PinDirection::Output, i, "Result"}}));
    classes.Register(MakeClass("Print", "Function",
                               {{PinDirection::Input, exec, "Exec"},
                                {PinDirection::Input, s, "Text"},
                                {PinDirection::Output, exec, "Then"}}));

    Graph g(types);
    const NodeId makeInt = g.AddNode(*classes.Find("MakeInt"), 0.0f, 0.0f);
    const NodeId addInt = g.AddNode(*classes.Find("AddInt"), 100.0f, 0.0f);

    const Node* mi = g.FindNode(makeInt);
    Check(mi != nullptr && mi->properties.size() == 1, "MakeInt spawned with 1 property");
    Check(mi != nullptr && mi->properties[0] == Value::Int(7), "property default copied");

    const PinId miOut = mi->outputs[0].id;
    const PinId addA = g.FindNode(addInt)->inputs[0].id;
    Check(g.CanConnect(miOut, addA), "int output -> int input connectable");

    const LinkId link = g.AddLink(miOut, addA);
    Check(link != INVALID_ID && g.Links().size() == 1, "link created");

    // Type mismatch: int output -> string input.
    const NodeId print = g.AddNode(*classes.Find("Print"), 200.0f, 0.0f);
    const PinId printText = g.FindNode(print)->inputs[1].id;
    Check(!g.CanConnect(miOut, printText), "int -> string rejected");

    // Removing a node drops its links.
    g.RemoveNode(addInt);
    Check(g.Links().empty(), "removing node clears its links");
}

static void TestExecCycle()
{
    TypeRegistry types;
    const TypeId exec = types.Builtin(TypeTag::Exec);
    NodeClassRegistry classes;
    classes.Register(MakeClass("Step", "Function",
                               {{PinDirection::Input, exec, "In"},
                                {PinDirection::Output, exec, "Out"}}));
    Graph g(types);
    const NodeId a = g.AddNode(*classes.Find("Step"), 0.0f, 0.0f);
    const NodeId b = g.AddNode(*classes.Find("Step"), 0.0f, 0.0f);

    const PinId aOut = g.FindNode(a)->outputs[0].id;
    const PinId bIn = g.FindNode(b)->inputs[0].id;
    const PinId bOut = g.FindNode(b)->outputs[0].id;
    const PinId aIn = g.FindNode(a)->inputs[0].id;

    Check(g.AddLink(aOut, bIn) != INVALID_ID, "exec a->b ok");
    Check(!g.CanConnect(bOut, aIn), "exec b->a would cycle, rejected");
}

int main()
{
    TestConnectionsAndSpawn();
    TestExecCycle();
    if (failCount == 0) {
        std::printf("model_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
