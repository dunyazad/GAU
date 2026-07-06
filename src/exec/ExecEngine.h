#pragma once

#include "model/NodeGraph.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class ExecEngine;

// Passed to node exec functions. Input/output/property indices are
// positions within the node's inputs/outputs/properties lists.
class ExecContext
{
public:
    ExecContext(ExecEngine& engine, const Node& node, int depth);

    const Node& GetNode() const { return node; }

    // Pulls a value: a connected input evaluates its source output, an
    // unconnected one falls back to the same-named scalar property,
    // else the pin type's default.
    Value GetInput(int inputIndex);

    // Scalar property value of this node instance (empty string when
    // out of range).
    Value GetProperty(int propertyIndex) const;

    void SetOutput(int outputIndex, Value value);

    // Synchronously runs the exec chain attached to outputs[outputIndex].
    void RunExecOutput(int outputIndex);

    // Wasm ABI index spaces: data pins and exec pins are numbered
    // separately. gau_input_*/gau_output_* address the nth non-exec pin
    // of a direction and gau_exec the nth exec output, so a function
    // body never has to know how many exec pins precede its data pins.
    Value GetDataInput(int dataIndex);
    void SetDataOutput(int dataIndex, Value value);
    void RunExecFlow(int execIndex);

    // True once the wasm function drove the flow itself via gau_exec;
    // the engine then skips the default continue-on-first-exec-output.
    bool ExecFlowTriggered() const { return execFlowTriggered; }

    void Log(const std::string& message);

private:
    ExecEngine& engine;
    const Node& node;
    int depth;
    bool execFlowTriggered = false;
};

using NodeExecFn = void (*)(ExecContext&);

// Executes a graph: exec pins flow sequentially from every Event Begin
// node; data pins pull-evaluate on demand (pure nodes re-evaluate every
// request). Log output goes through the injected sink, which the app
// fans out to the console, the log panel and future runtime views.
class ExecEngine
{
public:
    using LogFn = std::function<void(const std::string&)>;

    // Runaway guards: per-run node execution budget and data pull
    // recursion depth (data cycles are legal per the connection rules).
    static constexpr int MAX_EXECUTED_NODES = 10000;
    static constexpr int MAX_DATA_DEPTH = 64;

    ExecEngine(const NodeGraph& graph, LogFn logFn);

    // Runs from every Event Begin node. Returns false when no entry
    // node exists or the execution budget was exceeded.
    bool Run();

    // Side-effect-free pass for the editor: pull-evaluates every non-exec
    // output pin so pure/data-carrier chains report their current values.
    // Exec chains are not run, so PrintString and friends never fire.
    // Non-pure nodes' data outputs stay absent (only known after a real
    // run). Returns the evaluated PinId -> Value map.
    PinValueCache EvaluateDataPreview();

    void Log(const std::string& message);

private:
    friend class ExecContext;

    bool ExecuteNode(const Node& node);
    void RunNodeFunction(const Node& node, int depth);
    void RunDefaultBehavior(ExecContext& context, const Node& node);
    Value EvaluateOutputPin(PinId outputPinId, int depth);
    Value GetInputValue(const Node& node, int inputIndex, int depth);
    void SetOutputValue(const Node& node, int outputIndex, Value value);
    Value ReadCachedOutput(PinId pinId) const;
    void RunExecOutputChain(const Node& node, int outputIndex);
    static bool IsPure(const Node& node);

    const NodeGraph& graph;
    LogFn logFn;
    std::vector<std::pair<PinId, Value>> outputCache;
    int executedCount = 0;
    bool aborted = false;
};
