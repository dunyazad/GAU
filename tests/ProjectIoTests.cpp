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

static const char* DEFS = R"JSON(
{
  "types": [
    {"name": "Vector3f", "kind": "struct",
     "fields": [{"name": "x", "type": "float"}, {"name": "y", "type": "float"}]}
  ],
  "nodeClasses": [
    {"name": "MakeInt", "category": "Pure",
     "pins": [{"direction": "out", "type": "int", "name": "Value"}],
     "properties": [{"name": "Value", "container": "none", "type": "int", "default": 7}]},
    {"name": "PrintInt", "category": "Function",
     "pins": [{"direction": "in", "type": "exec", "name": "Exec"},
              {"direction": "in", "type": "int", "name": "Value"},
              {"direction": "out", "type": "exec", "name": "Then"}]}
  ]
}
)JSON";

static const char* GRAPH = R"JSON(
{
  "nodes": [
    {"id": "a", "class": "MakeInt", "x": 10, "y": 20, "properties": [9]},
    {"id": "b", "class": "PrintInt", "x": 200, "y": 20}
  ],
  "links": [{"fromId": "a", "fromPin": 0, "toId": "b", "toPin": 1}]
}
)JSON";

static void LoadInto(Project& p)
{
    std::vector<std::string> errors;
    ImportV1Definitions(DEFS, p.types, p.classes, errors);
    ImportV1Graph(GRAPH, *p.graph, p.classes, p.types, errors);
}

static void TestRoundTrip()
{
    Project a;
    LoadInto(a);

    const std::string exported = ExportProject(a);
    Check(!exported.empty(), "export non-empty");

    Project b;
    std::vector<std::string> errors;
    ImportV1Definitions(exported, b.types, b.classes, errors);
    ImportV1Graph(exported, *b.graph, b.classes, b.types, errors);
    Check(errors.empty(), "reimport without errors");

    Check(b.classes.Find("MakeInt") != nullptr, "MakeInt class survived");
    Check(b.classes.Find("PrintInt") != nullptr, "PrintInt class survived");
    Check(b.types.FindStruct("Vector3f") != nullptr, "Vector3f struct survived");
    Check(b.graph->Nodes().size() == a.graph->Nodes().size(), "node count preserved");
    Check(b.graph->Links().size() == a.graph->Links().size(), "link count preserved");

    bool propOk = false;
    for (const Node& n : b.graph->Nodes()) {
        if (n.className == "MakeInt") {
            propOk = n.properties.size() == 1 && n.properties[0] == Value::Int(9);
        }
    }
    Check(propOk, "MakeInt property override 9 round-tripped");
}

int main()
{
    TestRoundTrip();
    if (failCount == 0) {
        std::printf("project_io_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
