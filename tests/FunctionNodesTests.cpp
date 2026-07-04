#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/FunctionNodes.h"
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

// Pure function: Sum(A, B) = A + B, called through a data pin pull.
static void TestPureFunctionCall()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    const TypeId i = t.Builtin(TypeTag::Int);
    FunctionRegistry funcs;
    FunctionDef* def = funcs.Create(t, "AddTwo");
    def->inputs = {{"A", i}, {"B", i}};
    def->outputs = {{"Sum", i}};
    def->hasExec = false;
    RegisterFunctionNodes(classes, builtins, t, *def);

    // Body: (In.A, In.B) -> Add -> Out.Sum
    Graph& body = *def->body;
    const NodeId entry = body.AddNode(*classes.Find("AddTwo In"), 0, 0);
    const NodeId add = body.AddNode(*classes.Find("Add"), 0, 0);
    const NodeId ret = body.AddNode(*classes.Find("AddTwo Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;
    body.AddLink(body.FindNode(entry)->outputs[0].id, body.FindNode(add)->inputs[0].id);
    body.AddLink(body.FindNode(entry)->outputs[1].id, body.FindNode(add)->inputs[1].id);
    body.AddLink(body.FindNode(add)->outputs[0].id, body.FindNode(ret)->inputs[0].id);

    // Main graph: EventBegin -> PrintInt(AddTwo(4, 5))
    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId m4 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId m5 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId call = g.AddNode(*classes.Find("AddTwo"), 0, 0);
    const NodeId print = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    g.FindNode(m4)->properties[0] = Value::Int(4);
    g.FindNode(m5)->properties[0] = Value::Int(5);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(m4)->outputs[0].id, g.FindNode(call)->inputs[0].id);
    g.AddLink(g.FindNode(m5)->outputs[0].id, g.FindNode(call)->inputs[1].id);
    g.AddLink(g.FindNode(call)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    const bool done = rt.Run(1000);
    Check(done && rt.State() == RunState::Done, "pure function run completes");
    Check(logs.size() == 1 && logs[0] == "9", "pure AddTwo(4,5) prints 9");
}

// Exec function: Doubler(N) returns N + N through exec flow, result marshalled
// back to the caller's output pin.
static void TestExecFunctionCall()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    const TypeId i = t.Builtin(TypeTag::Int);
    FunctionRegistry funcs;
    FunctionDef* def = funcs.Create(t, "Doubler");
    def->inputs = {{"N", i}};
    def->outputs = {{"R", i}};
    def->hasExec = true;
    RegisterFunctionNodes(classes, builtins, t, *def);

    // Body: In.Then -> Out.Exec; In.N -> Add(A,B) -> Out.R
    Graph& body = *def->body;
    const NodeId entry = body.AddNode(*classes.Find("Doubler In"), 0, 0);
    const NodeId add = body.AddNode(*classes.Find("Add"), 0, 0);
    const NodeId ret = body.AddNode(*classes.Find("Doubler Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;
    // Entry: outputs[0]=Then(exec), outputs[1]=N. Return: inputs[0]=Exec, inputs[1]=R.
    body.AddLink(body.FindNode(entry)->outputs[0].id, body.FindNode(ret)->inputs[0].id);
    body.AddLink(body.FindNode(entry)->outputs[1].id, body.FindNode(add)->inputs[0].id);
    body.AddLink(body.FindNode(entry)->outputs[1].id, body.FindNode(add)->inputs[1].id);
    body.AddLink(body.FindNode(add)->outputs[0].id, body.FindNode(ret)->inputs[1].id);

    // Main: EventBegin -> Doubler(7) -> PrintInt(R)
    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId m7 = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId call = g.AddNode(*classes.Find("Doubler"), 0, 0);
    const NodeId print = g.AddNode(*classes.Find("PrintInt"), 0, 0);
    g.FindNode(m7)->properties[0] = Value::Int(7);
    // Call: inputs[0]=Exec, inputs[1]=N. outputs[0]=Then, outputs[1]=R.
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(call)->inputs[0].id);
    g.AddLink(g.FindNode(m7)->outputs[0].id, g.FindNode(call)->inputs[1].id);
    g.AddLink(g.FindNode(call)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(call)->outputs[1].id, g.FindNode(print)->inputs[1].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    const bool done = rt.Run(1000);
    Check(done && rt.State() == RunState::Done, "exec function run completes");
    Check(logs.size() == 1 && logs[0] == "14", "exec Doubler(7) prints 14");
}

// Unbounded self-recursion hits the depth cap and reports it instead of
// crashing.
static void TestRecursionCap()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    RegisterBaseClasses(classes, t);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);

    FunctionRegistry funcs;
    FunctionDef* def = funcs.Create(t, "Loop");
    def->hasExec = true;
    RegisterFunctionNodes(classes, builtins, t, *def);

    // Body: In.Then -> Loop.Exec ; Loop.Then -> Out.Exec
    Graph& body = *def->body;
    const NodeId entry = body.AddNode(*classes.Find("Loop In"), 0, 0);
    const NodeId inner = body.AddNode(*classes.Find("Loop"), 0, 0);
    const NodeId ret = body.AddNode(*classes.Find("Loop Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;
    body.AddLink(body.FindNode(entry)->outputs[0].id, body.FindNode(inner)->inputs[0].id);
    body.AddLink(body.FindNode(inner)->outputs[0].id, body.FindNode(ret)->inputs[0].id);

    Graph g(t);
    const NodeId ev = g.AddNode(*classes.Find("EventBegin"), 0, 0);
    const NodeId call = g.AddNode(*classes.Find("Loop"), 0, 0);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(call)->inputs[0].id);

    std::vector<std::string> logs;
    Runtime rt(g, t, classes, builtins, [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(ev);
    rt.Run(1000);
    bool hitLimit = false;
    for (const std::string& m : logs) {
        if (m.find("recursion limit") != std::string::npos) {
            hitLimit = true;
            break;
        }
    }
    Check(hitLimit, "unbounded recursion reports the depth limit");
}

int main()
{
    TestPureFunctionCall();
    TestExecFunctionCall();
    TestRecursionCap();
    if (failCount == 0) {
        std::printf("function_nodes_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
