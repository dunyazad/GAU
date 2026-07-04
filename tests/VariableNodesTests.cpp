#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/Runtime.h"
#include "exec/VariableNodes.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "model/Variable.h"

#include <cstdio>
#include <string>
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

static NodeClass Cls(std::string name, std::string category, std::vector<PinDef> pins,
                     std::vector<PropertyDef> props = {})
{
    NodeClass c;
    c.name = std::move(name);
    c.category = std::move(category);
    c.pins = std::move(pins);
    c.properties = std::move(props);
    return c;
}

static void RegisterBase(NodeClassRegistry& classes, const TypeRegistry& t)
{
    const TypeId exec = t.Builtin(TypeTag::Exec);
    const TypeId i = t.Builtin(TypeTag::Int);
    classes.Register(Cls("EventBegin", "Event", {{PinDirection::Output, exec, "Then"}}));
    classes.Register(Cls("MakeInt", "Pure", {{PinDirection::Output, i, "Value"}},
                         {{"Value", i, Value::Int(0)}}));
    classes.Register(Cls("Add", "Pure",
                         {{PinDirection::Input, i, "A"},
                          {PinDirection::Input, i, "B"},
                          {PinDirection::Output, i, "Result"}}));
    classes.Register(Cls("PrintInt", "Function",
                         {{PinDirection::Input, exec, "Exec"},
                          {PinDirection::Input, i, "Value"},
                          {PinDirection::Output, exec, "Then"}}));
}

// EventBegin -> Set x=7 -> Set x=(Get x + 3) -> Print(Get x). Expects 10,
// proving the variable persists and Get reads the live value mid-run.
static void TestVariablePersist()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBase(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    VariableDef x{"x", t.Builtin(TypeTag::Int)};
    RegisterVariableNodes(classes, builtins, t, x);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId set1 = g.AddNode(*classes.Find("Set x"), 0, 0);
    const NodeId m7 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId set2 = g.AddNode(*classes.Find("Set x"), 0, 0);
    const NodeId add = g.AddNode(*classes.Find("Add"), 0, 0);
    const NodeId getA = g.AddNode(*classes.Find("Get x"), 0, 0);
    const NodeId m3 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId print = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    const NodeId getP = g.AddNode(*classes.Find("Get x"), 0, 0);
    g.FindNode(m7)->properties[0] = Value::Int(7);
    g.FindNode(m3)->properties[0] = Value::Int(3);

    // Exec chain.
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(set1)->inputs[0].id);
    g.AddLink(g.FindNode(set1)->outputs[0].id, g.FindNode(set2)->inputs[0].id);
    g.AddLink(g.FindNode(set2)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    // Data.
    g.AddLink(g.FindNode(m7)->outputs[0].id, g.FindNode(set1)->inputs[1].id);
    g.AddLink(g.FindNode(getA)->outputs[0].id, g.FindNode(add)->inputs[0].id);
    g.AddLink(g.FindNode(m3)->outputs[0].id, g.FindNode(add)->inputs[1].id);
    g.AddLink(g.FindNode(add)->outputs[0].id, g.FindNode(set2)->inputs[1].id);
    g.AddLink(g.FindNode(getP)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    rt.Run(1000);
    Check(logs == std::vector<std::string>{"10"}, "variable persists: 7 + 3 = 10");
}

// A fresh run resets variables (Get before any Set yields the type default).
static void TestVariableResetAndDefault()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBase(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    VariableDef x{"x", t.Builtin(TypeTag::Int)};
    RegisterVariableNodes(classes, builtins, t, x);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId print = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    const NodeId getP = g.AddNode(*classes.Find("Get x"), 0, 0);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(getP)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    rt.Run(1000);
    Check(logs == std::vector<std::string>{"0"}, "unset variable reads type default 0");
}

int main()
{
    TestVariablePersist();
    TestVariableResetAndDefault();
    if (failCount == 0) {
        std::printf("variable_nodes_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
