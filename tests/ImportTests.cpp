#include "core/TypeRegistry.h"
#include "io/V1Import.h"
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

static const char* DEFINITIONS = R"JSON(
{
  "types": [
    {"name": "Direction", "kind": "enum", "values": ["North", "South"]},
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
              {"direction": "out", "type": "exec", "name": "Then"}]},
    {"name": "MoveDir", "category": "Function",
     "pins": [{"direction": "in", "type": "Vector3f", "name": "Delta"}]}
  ]
}
)JSON";

static const char* GRAPH = R"JSON(
{
  "nodes": [
    {"id": "a", "class": "MakeInt", "x": 10, "y": 20, "properties": [9]},
    {"id": "b", "class": "PrintInt", "x": 200, "y": 20}
  ],
  "links": [
    {"fromId": "a", "fromPin": 0, "toId": "b", "toPin": 1}
  ]
}
)JSON";

static void TestDefinitions(TypeRegistry& types, NodeClassRegistry& classes)
{
    std::vector<std::string> errors;
    const ImportCounts counts = ImportV1Definitions(DEFINITIONS, types, classes, errors);
    Check(errors.empty(), "no definition errors");
    Check(counts.types == 2, "2 types imported");
    Check(counts.classes == 3, "3 classes imported");

    const StructDef* vec = types.FindStruct("Vector3f");
    Check(vec != nullptr && vec->fields.size() == 2, "Vector3f struct with 2 fields");

    const NodeClass* moveDir = classes.Find("MoveDir");
    Check(moveDir != nullptr, "MoveDir class found");
    if (moveDir != nullptr) {
        const TypeDesc* pinType = types.Resolve(moveDir->pins[0].type);
        Check(pinType != nullptr && pinType->tag == TypeTag::Struct, "Vector3f pin resolves");
    }

    const NodeClass* makeInt = classes.Find("MakeInt");
    Check(makeInt != nullptr && makeInt->properties.size() == 1
              && makeInt->properties[0].defaultValue == Value::Int(7),
          "MakeInt default 7 imported");
}

static void TestGraph(TypeRegistry& types, const NodeClassRegistry& classes)
{
    Graph graph(types);
    std::vector<std::string> errors;
    const bool ok = ImportV1Graph(GRAPH, graph, classes, types, errors);
    Check(ok, "graph import ok");
    Check(graph.Nodes().size() == 2, "2 nodes imported");
    Check(graph.Links().size() == 1, "1 link imported");

    // The MakeInt node's property override (9) came through.
    bool found = false;
    for (const Node& n : graph.Nodes()) {
        if (n.className == "MakeInt") {
            found = n.properties.size() == 1 && n.properties[0] == Value::Int(9);
        }
    }
    Check(found, "node property override 9 imported");
}

int main()
{
    TypeRegistry types;
    NodeClassRegistry classes;
    TestDefinitions(types, classes);
    TestGraph(types, classes);
    if (failCount == 0) {
        std::printf("import_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
