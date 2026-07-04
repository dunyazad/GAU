#include "exec/Builtins.h"
#include "exec/FunctionInterface.h"
#include "exec/FunctionNodes.h"
#include "exec/Runtime.h"
#include "model/Project.h"

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

static NodeId FindByClass(const Graph& g, const std::string& c)
{
    for (const Node& n : g.Nodes()) {
        if (n.className == c) {
            return n.id;
        }
    }
    return INVALID_ID;
}

// Builds Sum2(A,B)=A+B as a real function with a body, then adds a third input
// C and rewires the body so the result becomes A+B+C. Verifies the added
// parameter flows end to end and existing pins/links survived.
static void TestAddInputParam()
{
    Project p;
    RegisterBase(p.classes, p.types);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    const TypeId i = p.types.Builtin(TypeTag::Int);

    FunctionDef* def = p.functions.Create(p.types, "Sum2");
    def->inputs = {{"A", i}, {"B", i}};
    def->outputs = {{"R", i}};
    def->hasExec = false;
    RegisterFunctionNodes(p.classes, builtins, p.types, *def);

    Graph& body = *def->body;
    const NodeId entry = body.AddNode(*p.classes.Find("Sum2 In"), 0, 0);
    const NodeId add1 = body.AddNode(*p.classes.Find("Add"), 0, 0);
    const NodeId ret = body.AddNode(*p.classes.Find("Sum2 Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;
    body.AddLink(body.FindNode(entry)->outputs[0].id, body.FindNode(add1)->inputs[0].id);
    body.AddLink(body.FindNode(entry)->outputs[1].id, body.FindNode(add1)->inputs[1].id);
    body.AddLink(body.FindNode(add1)->outputs[0].id, body.FindNode(ret)->inputs[0].id);

    Graph& g = *p.graph;
    const NodeId ev = g.AddNode(*p.classes.Find("EventBegin"), 0, 0);
    const NodeId m4 = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId m5 = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId call = g.AddNode(*p.classes.Find("Sum2"), 0, 0);
    const NodeId print = g.AddNode(*p.classes.Find("PrintInt"), 0, 0);
    g.FindNode(m4)->properties[0] = Value::Int(4);
    g.FindNode(m5)->properties[0] = Value::Int(5);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(m4)->outputs[0].id, g.FindNode(call)->inputs[0].id);
    g.AddLink(g.FindNode(m5)->outputs[0].id, g.FindNode(call)->inputs[1].id);
    g.AddLink(g.FindNode(call)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    const auto run = [&]() {
        std::vector<std::string> logs;
        Runtime rt(g, p.types, p.classes, builtins,
                   [&logs](const std::string& m) { logs.push_back(m); });
        rt.Start(FindByClass(g, "EventBegin"));
        rt.Run(1000);
        return logs;
    };
    Check(run() == std::vector<std::string>{"9"}, "baseline Sum2(4,5) = 9");

    // Add input C.
    AddFunctionParam(*def, false, "C", i, p.classes, builtins, p.types, p);
    Check(def->inputs.size() == 3, "function has three inputs");
    Check(g.FindNode(call)->inputs.size() == 3, "call node grew to three inputs");
    Check(body.FindNode(entry)->outputs.size() == 3, "entry node grew to three outputs");

    // Rewire the body: result = (A+B) + C.
    const NodeId add2 = body.AddNode(*p.classes.Find("Add"), 0, 0);
    body.AddLink(body.FindNode(add1)->outputs[0].id, body.FindNode(add2)->inputs[0].id);
    body.AddLink(body.FindNode(entry)->outputs[2].id, body.FindNode(add2)->inputs[1].id);
    body.AddLink(body.FindNode(add2)->outputs[0].id, body.FindNode(ret)->inputs[0].id);

    // Feed C = 100 on the call.
    const NodeId m100 = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    g.FindNode(m100)->properties[0] = Value::Int(100);
    g.AddLink(g.FindNode(m100)->outputs[0].id, g.FindNode(call)->inputs[2].id);

    Check(run() == std::vector<std::string>{"109"}, "Sum2(4,5,100) = 109 after adding input");
}

static void TestAddOutputParamKeepsRunning()
{
    Project p;
    RegisterBase(p.classes, p.types);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    const TypeId i = p.types.Builtin(TypeTag::Int);

    FunctionDef* def = p.functions.Create(p.types, "Id");
    def->inputs = {{"A", i}};
    def->outputs = {{"R", i}};
    def->hasExec = false;
    RegisterFunctionNodes(p.classes, builtins, p.types, *def);
    Graph& body = *def->body;
    const NodeId entry = body.AddNode(*p.classes.Find("Id In"), 0, 0);
    const NodeId ret = body.AddNode(*p.classes.Find("Id Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;
    body.AddLink(body.FindNode(entry)->outputs[0].id, body.FindNode(ret)->inputs[0].id);

    Graph& g = *p.graph;
    const NodeId ev = g.AddNode(*p.classes.Find("EventBegin"), 0, 0);
    const NodeId m7 = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId call = g.AddNode(*p.classes.Find("Id"), 0, 0);
    const NodeId print = g.AddNode(*p.classes.Find("PrintInt"), 0, 0);
    g.FindNode(m7)->properties[0] = Value::Int(7);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(m7)->outputs[0].id, g.FindNode(call)->inputs[0].id);
    g.AddLink(g.FindNode(call)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    AddFunctionParam(*def, true, "R2", i, p.classes, builtins, p.types, p);
    Check(def->outputs.size() == 2, "function has two outputs");
    Check(g.FindNode(call)->outputs.size() == 2, "call node grew to two outputs");
    Check(body.FindNode(ret)->inputs.size() == 2, "return node grew to two inputs");

    std::vector<std::string> logs;
    Runtime rt(g, p.types, p.classes, builtins,
               [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(FindByClass(g, "EventBegin"));
    rt.Run(1000);
    Check(logs == std::vector<std::string>{"7"}, "existing output still yields 7");
}

// Adding then removing the last input returns the function to its original
// shape and behavior; removing a middle param reindexes the rest.
static void TestRemoveParam()
{
    Project p;
    RegisterBase(p.classes, p.types);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    const TypeId i = p.types.Builtin(TypeTag::Int);

    FunctionDef* def = p.functions.Create(p.types, "Sum2");
    def->inputs = {{"A", i}, {"B", i}};
    def->outputs = {{"R", i}};
    def->hasExec = false;
    RegisterFunctionNodes(p.classes, builtins, p.types, *def);
    Graph& body = *def->body;
    const NodeId entry = body.AddNode(*p.classes.Find("Sum2 In"), 0, 0);
    const NodeId add1 = body.AddNode(*p.classes.Find("Add"), 0, 0);
    const NodeId ret = body.AddNode(*p.classes.Find("Sum2 Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;
    body.AddLink(body.FindNode(entry)->outputs[0].id, body.FindNode(add1)->inputs[0].id);
    body.AddLink(body.FindNode(entry)->outputs[1].id, body.FindNode(add1)->inputs[1].id);
    body.AddLink(body.FindNode(add1)->outputs[0].id, body.FindNode(ret)->inputs[0].id);

    Graph& g = *p.graph;
    const NodeId ev = g.AddNode(*p.classes.Find("EventBegin"), 0, 0);
    const NodeId m4 = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId m5 = g.AddNode(*p.classes.Find("MakeInt"), 0, 0);
    const NodeId call = g.AddNode(*p.classes.Find("Sum2"), 0, 0);
    const NodeId print = g.AddNode(*p.classes.Find("PrintInt"), 0, 0);
    g.FindNode(m4)->properties[0] = Value::Int(4);
    g.FindNode(m5)->properties[0] = Value::Int(5);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(m4)->outputs[0].id, g.FindNode(call)->inputs[0].id);
    g.AddLink(g.FindNode(m5)->outputs[0].id, g.FindNode(call)->inputs[1].id);
    g.AddLink(g.FindNode(call)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    const auto run = [&]() {
        std::vector<std::string> logs;
        Runtime rt(g, p.types, p.classes, builtins,
                   [&logs](const std::string& m) { logs.push_back(m); });
        rt.Start(FindByClass(g, "EventBegin"));
        rt.Run(1000);
        return logs;
    };

    AddFunctionParam(*def, false, "C", i, p.classes, builtins, p.types, p);
    Check(def->inputs.size() == 3 && g.FindNode(call)->inputs.size() == 3, "grew to three inputs");

    // Remove the last input (C): shape and behavior return to the original.
    RemoveFunctionParam(*def, false, 2, p.classes, builtins, p.types, p);
    Check(def->inputs.size() == 2, "back to two inputs");
    Check(g.FindNode(call)->inputs.size() == 2, "call node back to two inputs");
    Check(body.FindNode(entry)->outputs.size() == 2, "entry node back to two outputs");
    Check(run() == std::vector<std::string>{"9"}, "still runs 9 after add+remove");

    // Remove a middle input (A at index 0): B shifts down, C-less.
    RemoveFunctionParam(*def, false, 0, p.classes, builtins, p.types, p);
    Check(def->inputs.size() == 1 && def->inputs[0].name == "B", "middle removal keeps B");
    Check(g.FindNode(call)->inputs.size() == 1, "call node has one input after middle removal");
}

int main()
{
    TestAddInputParam();
    TestAddOutputParamKeepsRunning();
    TestRemoveParam();
    if (failCount == 0) {
        std::printf("function_interface_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
