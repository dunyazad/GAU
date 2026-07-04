#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/FunctionNodes.h"
#include "exec/FunctionOps.h"
#include "exec/Runtime.h"
#include "io/ProjectExport.h"
#include "io/V1Import.h"
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

static NodeId FindByClass(const Graph& g, const std::string& className)
{
    for (const Node& n : g.Nodes()) {
        if (n.className == className) {
            return n.id;
        }
    }
    return INVALID_ID;
}

// A project holding a collapsed function is exported to JSON, reloaded into a
// fresh project (defs, functions, graph), rebound to behaviors, and run.
static void TestFunctionRoundTrip()
{
    Project a;
    RegisterBaseClasses(a.classes, a.types);
    BuiltinRegistry builtinsA;
    RegisterDemoBuiltins(builtinsA);

    const NodeId ev = a.graph->AddNode(*a.classes.Find("EventBegin"), 0, 0);
    const NodeId m4 = a.graph->AddNode(*a.classes.Find("MakeInt"), 0, 100);
    const NodeId m5 = a.graph->AddNode(*a.classes.Find("MakeInt"), 0, 200);
    const NodeId add = a.graph->AddNode(*a.classes.Find("Add"), 150, 150);
    const NodeId print = a.graph->AddNode(*a.classes.Find("PrintInt"), 300, 0);
    a.graph->FindNode(m4)->properties[0] = Value::Int(4);
    a.graph->FindNode(m5)->properties[0] = Value::Int(5);
    a.graph->AddLink(a.graph->FindNode(ev)->outputs[0].id, a.graph->FindNode(print)->inputs[0].id);
    a.graph->AddLink(a.graph->FindNode(m4)->outputs[0].id, a.graph->FindNode(add)->inputs[0].id);
    a.graph->AddLink(a.graph->FindNode(m5)->outputs[0].id, a.graph->FindNode(add)->inputs[1].id);
    a.graph->AddLink(a.graph->FindNode(add)->outputs[0].id, a.graph->FindNode(print)->inputs[1].id);
    (void)ev;

    CollapseSelection(*a.graph, a.types, a.classes, builtinsA, a.functions, {add}, "AddFn");
    const std::string exported = ExportProject(a);
    Check(exported.find("AddFn") != std::string::npos, "export mentions AddFn");

    // Reload into a fresh project.
    Project b;
    std::vector<std::string> errors;
    ImportV1Definitions(exported, b.types, b.classes, errors);
    ImportFunctions(exported, b.functions, b.classes, b.types, errors);
    ImportV1Graph(exported, *b.graph, b.classes, b.types, errors);
    Check(errors.empty(), "reload without errors");

    const FunctionDef* def = b.functions.Find("AddFn");
    Check(def != nullptr, "AddFn function reloaded");
    Check(def != nullptr && def->inputs.size() == 2 && def->outputs.size() == 1,
          "AddFn interface (2 in, 1 out) survived");
    Check(def != nullptr && def->entryNode != INVALID_ID && def->returnNode != INVALID_ID,
          "AddFn entry/return located");

    // Rebind runtime behaviors and run the reloaded graph.
    BuiltinRegistry builtinsB;
    RegisterDemoBuiltins(builtinsB);
    for (const auto& fp : b.functions.All()) {
        RegisterFunctionNodes(b.classes, builtinsB, b.types, *fp);
    }

    const NodeId evB = FindByClass(*b.graph, "EventBegin");
    std::vector<std::string> logs;
    Runtime rt(*b.graph, b.types, b.classes, builtinsB,
               [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(evB);
    rt.Run(1000);
    Check(logs == std::vector<std::string>{"9"}, "reloaded collapsed graph prints 9");
}

int main()
{
    TestFunctionRoundTrip();
    if (failCount == 0) {
        std::printf("function_serialize_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
