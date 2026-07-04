#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/FunctionOps.h"
#include "exec/Runtime.h"
#include "model/Function.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

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

static void RegisterBaseClasses(NodeClassRegistry& classes, const TypeRegistry& t)
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

// Shared scene: EventBegin -> PrintInt( Add(4, 5) ). Returns the node ids the
// tests need. Prints 9 when run.
struct Scene
{
    NodeId ev;
    NodeId m4;
    NodeId m5;
    NodeId add;
    NodeId print;
};

static Scene BuildScene(Graph& g, const NodeClassRegistry& classes)
{
    Scene s;
    s.ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    s.m4 = g.AddNode(*classes.Find("MakeInt"), 0, 100);
    s.m5 = g.AddNode(*classes.Find("MakeInt"), 0, 200);
    s.add = g.AddNode(*classes.Find("Add"), 150, 150);
    s.print = g.AddNode(*classes.Find("PrintInt"), 300, 0);
    g.FindNode(s.m4)->properties[0] = Value::Int(4);
    g.FindNode(s.m5)->properties[0] = Value::Int(5);
    g.AddLink(g.FindNode(s.ev)->outputs[0].id, g.FindNode(s.print)->inputs[0].id);
    g.AddLink(g.FindNode(s.m4)->outputs[0].id, g.FindNode(s.add)->inputs[0].id);
    g.AddLink(g.FindNode(s.m5)->outputs[0].id, g.FindNode(s.add)->inputs[1].id);
    g.AddLink(g.FindNode(s.add)->outputs[0].id, g.FindNode(s.print)->inputs[1].id);
    return s;
}

static std::vector<std::string> RunFrom(Graph& g, TypeRegistry& t, NodeClassRegistry& classes,
                                        BuiltinRegistry& builtins, NodeId entry)
{
    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(entry);
    rt.Run(1000);
    return logs;
}

static void TestCollapsePure()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    FunctionRegistry funcs;

    Graph g(t);
    const Scene s = BuildScene(g, classes);
    Check(RunFrom(g, t, classes, builtins, s.ev) == std::vector<std::string>{"9"},
          "baseline prints 9");

    const NodeId call = CollapseSelection(g, t, classes, builtins, funcs, {s.add}, "AddFn");
    Check(call != INVALID_ID, "collapse returns a call node");
    Check(funcs.Find("AddFn") != nullptr, "function AddFn registered");
    Check(g.FindNode(s.add) == nullptr, "original Add removed from main");
    Check(classes.Find("AddFn") != nullptr, "call class generated");

    Check(RunFrom(g, t, classes, builtins, s.ev) == std::vector<std::string>{"9"},
          "collapsed pure function still prints 9");
}

static void TestCollapseExec()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    FunctionRegistry funcs;

    Graph g(t);
    const Scene s = BuildScene(g, classes);

    // Collapse the exec node PrintInt into an exec function.
    const NodeId call = CollapseSelection(g, t, classes, builtins, funcs, {s.print}, "Printer");
    Check(call != INVALID_ID, "exec collapse returns a call node");
    const FunctionDef* def = funcs.Find("Printer");
    Check(def != nullptr && def->hasExec, "Printer is an exec function");
    Check(def != nullptr && def->inputs.size() == 1 && def->outputs.empty(),
          "Printer exposes one input, no output");

    Check(RunFrom(g, t, classes, builtins, s.ev) == std::vector<std::string>{"9"},
          "collapsed exec function still prints 9");
}

static void TestExpandRoundTrip()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    FunctionRegistry funcs;

    Graph g(t);
    const Scene s = BuildScene(g, classes);
    const std::size_t baseNodes = g.Nodes().size();

    const NodeId call = CollapseSelection(g, t, classes, builtins, funcs, {s.add}, "AddFn");
    Check(call != INVALID_ID, "collapse ok");

    const bool expanded = ExpandCall(g, t, classes, funcs, call);
    Check(expanded, "expand ok");
    Check(g.FindNode(call) == nullptr, "call node removed after expand");
    Check(g.Nodes().size() == baseNodes, "node count restored after expand");

    Check(RunFrom(g, t, classes, builtins, s.ev) == std::vector<std::string>{"9"},
          "expanded graph still prints 9");
}

static void TestCollapseGuards()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    FunctionRegistry funcs;

    Graph g(t);
    const Scene s = BuildScene(g, classes);
    Check(CollapseSelection(g, t, classes, builtins, funcs, {}, "Empty") == INVALID_ID,
          "empty selection rejected");
    CollapseSelection(g, t, classes, builtins, funcs, {s.add}, "AddFn");
    Check(CollapseSelection(g, t, classes, builtins, funcs, {s.m4}, "AddFn") == INVALID_ID,
          "duplicate name rejected");
}

int main()
{
    TestCollapsePure();
    TestCollapseExec();
    TestExpandRoundTrip();
    TestCollapseGuards();
    if (failCount == 0) {
        std::printf("function_ops_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
