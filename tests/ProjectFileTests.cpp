#include "exec/Builtins.h"
#include "exec/FunctionNodes.h"
#include "exec/FunctionOps.h"
#include "exec/Runtime.h"
#include "exec/VariableNodes.h"
#include "io/ProjectExport.h"
#include "io/ProjectFile.h"
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

static NodeId FindByClass(const Graph& g, const std::string& className)
{
    for (const Node& n : g.Nodes()) {
        if (n.className == className) {
            return n.id;
        }
    }
    return INVALID_ID;
}

// A project with a collapsed function AND a local variable is saved to a
// file, reloaded, rebound to behaviors, and run. Exercises FR-PRJ-1/3 end to
// end on top of FR-REU-1/2.
static void TestFileRoundTrip()
{
    Project a;
    RegisterBase(a.classes, a.types);
    BuiltinRegistry builtinsA;
    RegisterDemoBuiltins(builtinsA);
    a.variables.push_back(VariableDef{"gain", a.types.Builtin(TypeTag::Int)});
    RegisterVariableNodes(a.classes, builtinsA, a.types, a.variables[0]);

    Graph& g = *a.graph;
    const NodeId ev = g.AddNode(*a.classes.Find("EventBegin"), 0, 0);
    const NodeId setG = g.AddNode(*a.classes.Find("Set gain"), 0, 0);
    const NodeId m4 = g.AddNode(*a.classes.Find("MakeInt"), 0, 0);
    const NodeId m5 = g.AddNode(*a.classes.Find("MakeInt"), 0, 0);
    const NodeId add = g.AddNode(*a.classes.Find("Add"), 0, 0);
    const NodeId print = g.AddNode(*a.classes.Find("PrintInt"), 0, 0);
    const NodeId getG = g.AddNode(*a.classes.Find("Get gain"), 0, 0);
    g.FindNode(m4)->properties[0] = Value::Int(4);
    g.FindNode(m5)->properties[0] = Value::Int(5);
    g.AddLink(g.FindNode(ev)->outputs[0].id, g.FindNode(setG)->inputs[0].id);
    g.AddLink(g.FindNode(setG)->outputs[0].id, g.FindNode(print)->inputs[0].id);
    g.AddLink(g.FindNode(m4)->outputs[0].id, g.FindNode(add)->inputs[0].id);
    g.AddLink(g.FindNode(m5)->outputs[0].id, g.FindNode(add)->inputs[1].id);
    g.AddLink(g.FindNode(add)->outputs[0].id, g.FindNode(setG)->inputs[1].id);
    g.AddLink(g.FindNode(getG)->outputs[0].id, g.FindNode(print)->inputs[1].id);

    // Fold Add into a function; the Call node keeps feeding Set gain.
    CollapseSelection(g, a.types, a.classes, builtinsA, a.functions, {add}, "AddFn");

    const std::string path = "gau_project_test.tmp.json";
    Check(SaveProjectFile(path, a), "save project file");

    Project b;
    std::vector<std::string> errors;
    Check(LoadProjectFile(path, b, errors), "load project file");
    Check(errors.empty(), "load without errors");
    Check(b.functions.Find("AddFn") != nullptr, "function survived file round-trip");
    Check(b.variables.size() == 1 && b.variables[0].name == "gain", "variable survived");

    // Confirm the schema version is what we wrote.
    Project schemaCheck;
    const int version = LoadProjectText(ExportProject(b), schemaCheck, errors);
    Check(version == PROJECT_SCHEMA_VERSION, "schema version round-trips");

    // Rebind behaviors and run.
    BuiltinRegistry builtinsB;
    RegisterDemoBuiltins(builtinsB);
    for (const auto& fp : b.functions.All()) {
        RegisterFunctionNodes(b.classes, builtinsB, b.types, *fp);
    }
    for (const VariableDef& v : b.variables) {
        RegisterVariableNodes(b.classes, builtinsB, b.types, v);
    }

    std::vector<std::string> logs;
    Runtime rt(*b.graph, b.types, b.classes, builtinsB,
               [&logs](const std::string& m) { logs.push_back(m); });
    rt.Start(FindByClass(*b.graph, "EventBegin"));
    rt.Run(1000);
    Check(logs == std::vector<std::string>{"9"}, "reloaded project runs: gain = AddFn(4,5) = 9");

    std::remove(path.c_str());
}

int main()
{
    TestFileRoundTrip();
    if (failCount == 0) {
        std::printf("project_file_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
