#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/Runtime.h"
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

static void RegisterClasses(NodeClassRegistry& classes, const TypeRegistry& t)
{
    const TypeId exec = t.Builtin(TypeTag::Exec);
    const TypeId i = t.Builtin(TypeTag::Int);
    const TypeId b = t.Builtin(TypeTag::Bool);
    classes.Register(Cls("EventBegin", "Event", {{PinDirection::Output, exec, "Then"}}));
    classes.Register(Cls("MakeInt", "Pure", {{PinDirection::Output, i, "Value"}},
                         {{"Value", i, Value::Int(0)}}));
    classes.Register(Cls("MakeBool", "Pure", {{PinDirection::Output, b, "Value"}},
                         {{"Value", b, Value::Bool(false)}}));
    classes.Register(Cls("Add", "Pure",
                         {{PinDirection::Input, i, "A"},
                          {PinDirection::Input, i, "B"},
                          {PinDirection::Output, i, "Result"}}));
    classes.Register(Cls("PrintInt", "Function",
                         {{PinDirection::Input, exec, "Exec"},
                          {PinDirection::Input, i, "Value"},
                          {PinDirection::Output, exec, "Then"}}));
    classes.Register(Cls("Branch", "FlowControl",
                         {{PinDirection::Input, exec, "Exec"},
                          {PinDirection::Input, b, "Cond"},
                          {PinDirection::Output, exec, "True"},
                          {PinDirection::Output, exec, "False"}}));
}

static void TestDataFlowAndExec()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId mi2 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId mi3 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId add = g.AddNode(*classes.Find("Add"), 0, 0);
    const NodeId print = g.AddNode(*classes.Find("PrintInt"), 0, 0);

    g.FindNode(mi2)->properties[0] = Value::Int(2);
    g.FindNode(mi3)->properties[0] = Value::Int(3);

    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(add)->outputs[0].id, g.FindNode(print)->inputs[1].id);
    g.AddLink(g.FindNode(mi2)->outputs[0].id, g.FindNode(add)->inputs[0].id);
    g.AddLink(g.FindNode(mi3)->outputs[0].id, g.FindNode(add)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    const bool done = rt.Run(1000);
    Check(done && rt.State() == RunState::Done, "run completes");
    Check(logs.size() == 1 && logs[0] == "5", "prints 2 + 3 = 5");
}

static void TestBranch()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId br = g.AddNode(*classes.Find("Branch"), 0, 0);
    const NodeId cond = g.AddNode(*classes.Find("MakeBool"), 0, 0);
    const NodeId one = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId pTrue = g.AddNode(*classes.Find("PrintInt"), 0, 0);

    g.FindNode(cond)->properties[0] = Value::Bool(true);
    g.FindNode(one)->properties[0] = Value::Int(1);

    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(br)->inputs[0].id);
    g.AddLink(g.FindNode(cond)->outputs[0].id, g.FindNode(br)->inputs[1].id);
    g.AddLink(g.FindNode(br)->outputs[0].id, g.FindNode(pTrue)->inputs[0].id); // True
    g.AddLink(g.FindNode(one)->outputs[0].id, g.FindNode(pTrue)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    rt.Run(1000);
    Check(logs.size() == 1 && logs[0] == "1", "branch true path prints 1");
}

static void TestBreakpoint()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId mi = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId print = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    g.FindNode(mi)->properties[0] = Value::Int(9);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(mi)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.AddBreakpoint(print);
    rt.Start(ev);
    rt.Run(1000);
    Check(rt.State() == RunState::Paused, "paused at breakpoint");
    Check(logs.empty(), "print not executed while paused");
    Check(rt.CurrentNode() == print, "paused on the print node");

    rt.Continue();
    const bool done = rt.Run(1000);
    Check(done && logs.size() == 1 && logs[0] == "9", "resumes and prints 9");
}

static void TestStepLimitError()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId p1 = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    const NodeId p2 = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    const NodeId p3 = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(p1)->inputs[0].id);
    g.AddLink(g.FindNode(p1)->outputs[0].id, g.FindNode(p2)->inputs[0].id);
    g.AddLink(g.FindNode(p2)->outputs[0].id, g.FindNode(p3)->inputs[0].id);

    Runtime rt(g, t, classes, builtins, nullptr);
    rt.Start(ev);
    const bool done = rt.Run(2);
    Check(!done && rt.State() == RunState::Error, "step limit ends in error");
    Check(rt.Error().kind == RunErrorKind::StepLimitExceeded, "error kind is StepLimitExceeded");
    Check(!rt.Error().message.empty(), "error carries a message");
}

static void TestNodeNotFoundError()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    Graph g(t);
    Runtime rt(g, t, classes, builtins, nullptr);
    rt.Start(9999); // no such node
    rt.Step();
    Check(rt.State() == RunState::Error, "missing entry node errors");
    Check(rt.Error().kind == RunErrorKind::NodeNotFound && rt.Error().node == 9999,
          "error identifies the missing node");
}

int main()
{
    TestDataFlowAndExec();
    TestBranch();
    TestBreakpoint();
    TestStepLimitError();
    TestNodeNotFoundError();
    if (failCount == 0) {
        std::printf("exec_vm_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
