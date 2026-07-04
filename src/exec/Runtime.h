#pragma once

// v2 execution runtime: a suspendable VM over a v2 Graph. Exec flow steps
// one node at a time (breakpoint-aware); data pins pull-evaluate core
// Values, so structs and containers flow through links without special
// casing.

#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include "core/Value.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace gau {

class TypeRegistry;

enum class RunState
{
    Idle,
    Running,
    Paused,
    Done,
    Error,
};

enum class RunErrorKind
{
    None,
    NodeNotFound,       // exec advanced to a node id that no longer exists
    StepLimitExceeded,  // Run hit its step budget while still running
};

// Structured diagnostic for a failed run (SRS FR-EXE-4): what went wrong and
// the node the runtime was on when it did.
struct RunError
{
    RunErrorKind kind = RunErrorKind::None;
    NodeId node = INVALID_ID;
    std::string message;
};

struct NodeEval;
using NodeFn = std::function<void(NodeEval&)>;

// Maps class names to node behaviors.
class BuiltinRegistry
{
public:
    void Register(std::string name, NodeFn fn);
    const NodeFn* Find(const std::string& name) const;

private:
    std::vector<std::pair<std::string, NodeFn>> fns;
};

class Runtime
{
public:
    using LogFn = std::function<void(const std::string&)>;

    Runtime(const Graph& graph, const TypeRegistry& types, const NodeClassRegistry& classes,
            const BuiltinRegistry& builtins, LogFn log);

    void AddBreakpoint(NodeId nodeId);
    void ClearBreakpoints();

    // Begins execution at an entry node (state -> Running).
    void Start(NodeId entryNode);
    // Executes one exec node, or pauses when the current node has a
    // breakpoint. Returns the new state.
    RunState Step();
    // Resumes past the breakpoint the runtime is paused on.
    void Continue();
    // Steps until Paused/Done/Error or maxSteps reached. True if Done.
    bool Run(int maxSteps);

    RunState State() const { return state; }
    NodeId CurrentNode() const { return pcNode; }
    // Diagnostic for the last error; kind is None unless State() is Error.
    const RunError& Error() const { return lastError; }

    // Evaluates a pin's value for a watch view (input pins pull their
    // source; output pins evaluate their owner when pure).
    Value EvalPin(PinId pinId);

    // Hooks used by NodeEval.
    Value EvalInputPin(PinId inputPinId);
    void SetOutput(PinId outputPinId, Value value);
    void ChooseNext(PinId execOutputPinId);
    const Value& Property(const Node& node, int index) const;
    void Log(const std::string& message) const;

    // Function-call marshalling. A Call node's behavior seeds the params a
    // nested runtime hands to the body's Entry node, then reads the values
    // the body's Return node wrote back.
    void SetParamsIn(std::vector<Value> params);
    Value ParamIn(int index) const;
    void SetResult(int index, Value value);
    Value ResultOut(int index) const;
    // Runs one node's behavior directly (used to pull a pure function's
    // Return node). Does not advance exec flow.
    void EvalNode(NodeId nodeId);
    // Recursion guard threaded from caller to nested runtime.
    void SetCallDepth(int depth) { callDepth = depth; }
    int CallDepth() const { return callDepth; }

    // Graph-scoped local variables (SRS FR-REU-2). Set nodes write, Get
    // nodes read; values persist across steps and clear on Start.
    void SetVariable(const std::string& name, Value value);
    Value GetVariable(const std::string& name) const;

private:
    const NodeClass* ClassOf(const Node& node) const;
    bool IsExecPin(const Pin& pin) const;
    bool IsPureClass(const NodeClass& cls) const;
    void EvaluateNode(const Node& node);
    PinId DefaultNextExec(const Node& node) const;
    NodeId FollowExec(PinId execOutputPinId) const;
    Value CachedOutput(PinId outputPinId) const;
    // Latches an exec node's data outputs so a later node can pull them
    // after the per-step outputCache has been cleared.
    void LatchExecOutputs(const Node& node);

    const Graph* graph;
    const TypeRegistry* types;
    const NodeClassRegistry* classes;
    const BuiltinRegistry* builtins;
    LogFn log;

    std::vector<std::pair<PinId, Value>> outputCache;
    std::vector<std::pair<PinId, Value>> execOutputCache;
    std::vector<NodeId> breakpoints;
    std::vector<NodeId> evalStack;
    std::vector<Value> paramsIn;
    std::vector<Value> resultsOut;
    std::vector<std::pair<std::string, Value>> variables;

    RunState state = RunState::Idle;
    NodeId pcNode = INVALID_ID;
    PinId chosenExec = INVALID_ID;
    bool ignoreBreakOnce = false;
    int callDepth = 0;
    RunError lastError;
};

// Passed to a node behavior during evaluation. Indices refer to the
// node's ordered inputs/outputs/properties.
struct NodeEval
{
    Runtime* rt = nullptr;
    const Node* node = nullptr;

    Value In(int inputIndex) const;
    const Value& Prop(int propertyIndex) const;
    void Out(int outputIndex, Value value) const;
    void Then(int outputIndex) const;
};

} // namespace gau
