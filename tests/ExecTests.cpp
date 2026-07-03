#include "exec/ExecEngine.h"
#include "model/NodeClass.h"
#include "model/NodeGraph.h"

#include <cstdio>
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

int main()
{
    TestPrintChain();
    TestForLoopAndBranch();

    if (failCount == 0) {
        std::printf("ExecTests: all tests passed\n");
        return 0;
    }
    std::printf("ExecTests: %d check(s) failed\n", failCount);
    return 1;
}
