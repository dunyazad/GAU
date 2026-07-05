#include "core/TypeRegistry.h"
#include "exec/Runtime.h"
#include "exec/WasmHost.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
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

// Hand-assembled wasm module (same bytes as the v1 exec test) exporting "run":
//   gau_output_i32(0, gau_input_i32(0) + gau_input_i32(1))
static const unsigned char ADDER_WASM[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x0E, 0x03, 0x60, 0x01, 0x7F, 0x01, 0x7F,
    0x60, 0x02, 0x7F, 0x7F, 0x00, 0x60, 0x00, 0x00,
    0x02, 0x2A, 0x02,
    0x03, 0x65, 0x6E, 0x76, 0x0D, 0x67, 0x61, 0x75, 0x5F, 0x69, 0x6E,
    0x70, 0x75, 0x74, 0x5F, 0x69, 0x33, 0x32, 0x00, 0x00,
    0x03, 0x65, 0x6E, 0x76, 0x0E, 0x67, 0x61, 0x75, 0x5F, 0x6F, 0x75,
    0x74, 0x70, 0x75, 0x74, 0x5F, 0x69, 0x33, 0x32, 0x00, 0x01,
    0x03, 0x02, 0x01, 0x02,
    0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x02,
    0x0A, 0x11, 0x01, 0x0F, 0x00,
    0x41, 0x00,
    0x41, 0x00, 0x10, 0x00,
    0x41, 0x01, 0x10, 0x00,
    0x6A,
    0x10, 0x01,
    0x0B,
};

static NodeClass Cls(std::string name, std::string category, std::vector<PinDef> pins,
                     std::vector<PropertyDef> props, std::string execFn)
{
    NodeClass c;
    c.name = std::move(name);
    c.category = std::move(category);
    c.pins = std::move(pins);
    c.properties = std::move(props);
    c.execFn = std::move(execFn);
    return c;
}

// A wasm-backed Pure node (2 int inputs -> 1 int output, execFn "wasm:run")
// evaluates through the v2 Runtime and returns the sum, proving the NodeEval
// bridge feeds inputs to and reads outputs from the wasm export.
static void TestWasmNodeEval()
{
    const char* dir = "wasm_host_test_data";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    {
        std::ofstream file(std::string(dir) + "/adder.wasm", std::ios::binary);
        file.write(reinterpret_cast<const char*>(ADDER_WASM), sizeof(ADDER_WASM));
    }

    std::vector<std::string> wasmErrors;
    const int loaded = WasmHost::Instance().LoadModulesFromDirectory(dir, wasmErrors);
    for (const std::string& error : wasmErrors) {
        std::printf("  wasm error: %s\n", error.c_str());
    }
    Check(loaded == 1, "wasm module loaded");
    Check(WasmHost::Instance().HasFunction("run"), "wasm export found");

    TypeRegistry types;
    const TypeId i = types.Builtin(TypeTag::Int);

    NodeClassRegistry classes;
    classes.Register(Cls("MakeInt", "Pure", {{PinDirection::Output, i, "Value"}},
                         {{"Value", i, Value::Int(0)}}, ""));
    classes.Register(Cls("WasmAdd", "Pure",
                         {{PinDirection::Input, i, "A"},
                          {PinDirection::Input, i, "B"},
                          {PinDirection::Output, i, "Result"}},
                         {}, "wasm:run"));

    BuiltinRegistry builtins;
    builtins.Register("MakeInt", [](NodeEval& e) { e.Out(0, e.Prop(0)); });

    Graph g(types);
    const NodeId a = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId b = g.AddNode(*classes.Find("MakeInt"), 0, 0);
    const NodeId add = g.AddNode(*classes.Find("WasmAdd"), 0, 0);
    g.FindNode(a)->properties[0] = Value::Int(4);
    g.FindNode(b)->properties[0] = Value::Int(6);
    g.AddLink(g.FindNode(a)->outputs[0].id, g.FindNode(add)->inputs[0].id);
    g.AddLink(g.FindNode(b)->outputs[0].id, g.FindNode(add)->inputs[1].id);

    Runtime rt(g, types, classes, builtins, [](const std::string&) {});
    const Value result = rt.EvalPin(g.FindNode(add)->outputs[0].id);
    Check(result == Value::Int(10), "wasm add through v2 runtime yields 10");

    // A different input pair re-evaluates (pure node runs per request).
    g.FindNode(a)->properties[0] = Value::Int(20);
    Runtime rt2(g, types, classes, builtins, [](const std::string&) {});
    Check(rt2.EvalPin(g.FindNode(add)->outputs[0].id) == Value::Int(26), "re-eval yields 26");
}

int main()
{
    TestWasmNodeEval();
    if (failCount == 0) {
        std::printf("wasm_host_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
