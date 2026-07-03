#include "exec/ExecEngine.h"
#include "exec/WasmRuntime.h"
#include "model/NodeClass.h"
#include "model/NodeGraph.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

// Custom class bound to the native MakeString behavior, like a
// JSON-defined class with "execFn": "MakeString".
static const NodeClass TestMakeStringClass("Test MakeString", "Function", {
    {PinDirection::Input, PinType::Int, "arg0"},
    {PinDirection::Output, PinType::String, "out"},
}, {
    {"data", PropertyContainer::None, PinType::String, PinType::String,
     Value(std::string("value={0}")), {}, {}},
}, "MakeString");

static void TestPrintChain()
{
    NodeGraph graph;

    const NodeId beginId = graph.AddNode(*NodeClass::FindByName("Event Begin"), 0, 0);
    const NodeId printId = graph.AddNode(*NodeClass::FindByName("Print String"), 0, 0);
    const NodeId makeIntA = graph.AddNode(*NodeClass::FindByName("Make Int"), 0, 0);
    const NodeId makeIntB = graph.AddNode(*NodeClass::FindByName("Make Int"), 0, 0);
    const NodeId addId = graph.AddNode(*NodeClass::FindByName("Add"), 0, 0);
    const NodeId formatId = graph.AddNode(TestMakeStringClass, 0, 0);

    graph.FindNode(makeIntA)->propertyValues[0].scalar = Value(2);
    graph.FindNode(makeIntB)->propertyValues[0].scalar = Value(3);

    const Node* begin = graph.FindNode(beginId);
    const Node* print = graph.FindNode(printId);
    const Node* intA = graph.FindNode(makeIntA);
    const Node* intB = graph.FindNode(makeIntB);
    const Node* add = graph.FindNode(addId);
    const Node* format = graph.FindNode(formatId);

    graph.AddLink(begin->outputs[0].id, print->inputs[0].id);
    graph.AddLink(intA->outputs[0].id, add->inputs[0].id);
    graph.AddLink(intB->outputs[0].id, add->inputs[1].id);
    graph.AddLink(add->outputs[0].id, format->inputs[0].id);
    graph.AddLink(format->outputs[0].id, print->inputs[1].id);

    std::vector<std::string> log;
    ExecEngine engine(graph, [&log](const std::string& message) { log.push_back(message); });

    Check(engine.Run(), "run succeeds");
    Check(log.size() == 1, "one log line");
    Check(!log.empty() && log[0] == "value=5", "MakeString formats Add result");
}

static void TestForLoopAndBranch()
{
    NodeGraph graph;

    const NodeId beginId = graph.AddNode(*NodeClass::FindByName("Event Begin"), 0, 0);
    const NodeId loopId = graph.AddNode(*NodeClass::FindByName("For Loop"), 0, 0);
    const NodeId firstId = graph.AddNode(*NodeClass::FindByName("Make Int"), 0, 0);
    const NodeId lastId = graph.AddNode(*NodeClass::FindByName("Make Int"), 0, 0);
    const NodeId printId = graph.AddNode(*NodeClass::FindByName("Print String"), 0, 0);
    const NodeId doneId = graph.AddNode(*NodeClass::FindByName("Print String"), 0, 0);

    graph.FindNode(firstId)->propertyValues[0].scalar = Value(1);
    graph.FindNode(lastId)->propertyValues[0].scalar = Value(3);

    const Node* begin = graph.FindNode(beginId);
    const Node* loop = graph.FindNode(loopId);
    const Node* first = graph.FindNode(firstId);
    const Node* last = graph.FindNode(lastId);
    const Node* print = graph.FindNode(printId);
    const Node* done = graph.FindNode(doneId);

    graph.AddLink(begin->outputs[0].id, loop->inputs[0].id);
    graph.AddLink(first->outputs[0].id, loop->inputs[1].id);
    graph.AddLink(last->outputs[0].id, loop->inputs[2].id);
    graph.AddLink(loop->outputs[0].id, print->inputs[0].id);
    graph.AddLink(loop->outputs[2].id, done->inputs[0].id);

    std::vector<std::string> log;
    ExecEngine engine(graph, [&log](const std::string& message) { log.push_back(message); });

    Check(engine.Run(), "loop run succeeds");
    // Loop body prints "" three times, then Completed prints once.
    Check(log.size() == 4, "loop body 3x + completed 1x");
}

// Hand-assembled wasm module exporting "run":
//   gau_output_i32(0, gau_input_i32(0) + gau_input_i32(1))
static const unsigned char ADDER_WASM[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00,
    // types: (i32)->i32, (i32,i32)->(), ()->()
    0x01, 0x0E, 0x03, 0x60, 0x01, 0x7F, 0x01, 0x7F,
    0x60, 0x02, 0x7F, 0x7F, 0x00, 0x60, 0x00, 0x00,
    // imports: env.gau_input_i32, env.gau_output_i32
    0x02, 0x2A, 0x02,
    0x03, 0x65, 0x6E, 0x76, 0x0D, 0x67, 0x61, 0x75, 0x5F, 0x69, 0x6E,
    0x70, 0x75, 0x74, 0x5F, 0x69, 0x33, 0x32, 0x00, 0x00,
    0x03, 0x65, 0x6E, 0x76, 0x0E, 0x67, 0x61, 0x75, 0x5F, 0x6F, 0x75,
    0x74, 0x70, 0x75, 0x74, 0x5F, 0x69, 0x33, 0x32, 0x00, 0x01,
    // function: type 2
    0x03, 0x02, 0x01, 0x02,
    // export "run" = func 2
    0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x02,
    // code
    0x0A, 0x11, 0x01, 0x0F, 0x00,
    0x41, 0x00,             // i32.const 0 (output index)
    0x41, 0x00, 0x10, 0x00, // input(0)
    0x41, 0x01, 0x10, 0x00, // input(1)
    0x6A,                   // i32.add
    0x10, 0x01,             // output(...)
    0x0B,
};

static const NodeClass TestWasmAddClass("Test WasmAdd", "Pure", {
    {PinDirection::Input, PinType::Int, "A"},
    {PinDirection::Input, PinType::Int, "B"},
    {PinDirection::Output, PinType::Int, "Result"},
}, {}, "wasm:run");

static void TestWasmFunction()
{
    const char* dir = "exec_test_wasm";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    {
        std::ofstream file(std::string(dir) + "/adder.wasm", std::ios::binary);
        file.write(reinterpret_cast<const char*>(ADDER_WASM), sizeof(ADDER_WASM));
    }

    std::vector<std::string> wasmErrors;
    const int loaded = WasmRuntime::Instance().LoadModulesFromDirectory(dir, wasmErrors);
    for (const std::string& error : wasmErrors) {
        std::printf("  wasm error: %s\n", error.c_str());
    }
    Check(loaded == 1, "wasm module loaded");
    Check(WasmRuntime::Instance().HasFunction("run"), "wasm export found");

    NodeGraph graph;
    const NodeId beginId = graph.AddNode(*NodeClass::FindByName("Event Begin"), 0, 0);
    const NodeId printId = graph.AddNode(*NodeClass::FindByName("Print String"), 0, 0);
    const NodeId intA = graph.AddNode(*NodeClass::FindByName("Make Int"), 0, 0);
    const NodeId intB = graph.AddNode(*NodeClass::FindByName("Make Int"), 0, 0);
    const NodeId wasmId = graph.AddNode(TestWasmAddClass, 0, 0);
    const NodeId formatId = graph.AddNode(TestMakeStringClass, 0, 0);

    graph.FindNode(intA)->propertyValues[0].scalar = Value(4);
    graph.FindNode(intB)->propertyValues[0].scalar = Value(6);

    const Node* begin = graph.FindNode(beginId);
    const Node* print = graph.FindNode(printId);
    const Node* a = graph.FindNode(intA);
    const Node* b = graph.FindNode(intB);
    const Node* wasmNode = graph.FindNode(wasmId);
    const Node* format = graph.FindNode(formatId);

    graph.AddLink(begin->outputs[0].id, print->inputs[0].id);
    graph.AddLink(a->outputs[0].id, wasmNode->inputs[0].id);
    graph.AddLink(b->outputs[0].id, wasmNode->inputs[1].id);
    graph.AddLink(wasmNode->outputs[0].id, format->inputs[0].id);
    graph.AddLink(format->outputs[0].id, print->inputs[1].id);

    std::vector<std::string> log;
    ExecEngine engine(graph, [&log](const std::string& message) { log.push_back(message); });

    Check(engine.Run(), "wasm run succeeds");
    Check(log.size() == 1 && log[0] == "value=10", "wasm add result printed");
}

int main()
{
    TestPrintChain();
    TestForLoopAndBranch();
    TestWasmFunction();

    if (failCount == 0) {
        std::printf("ExecTests: all tests passed\n");
        return 0;
    }
    std::printf("ExecTests: %d check(s) failed\n", failCount);
    return 1;
}
