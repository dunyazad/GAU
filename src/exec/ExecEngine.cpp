#include "ExecEngine.h"
#include "BuiltinNodes.h"
#include "WasmRuntime.h"

ExecContext::ExecContext(ExecEngine& engine, const Node& node, int depth)
    : engine(engine)
    , node(node)
    , depth(depth)
{
}

Value ExecContext::GetInput(int inputIndex)
{
    return engine.GetInputValue(node, inputIndex, depth);
}

Value ExecContext::GetProperty(int propertyIndex) const
{
    if (propertyIndex < 0
        || propertyIndex >= static_cast<int>(node.propertyValues.size())) {
        return Value(std::string());
    }
    return node.propertyValues[static_cast<std::size_t>(propertyIndex)].scalar;
}

void ExecContext::SetOutput(int outputIndex, Value value)
{
    engine.SetOutputValue(node, outputIndex, std::move(value));
}

void ExecContext::RunExecOutput(int outputIndex)
{
    engine.RunExecOutputChain(node, outputIndex);
}

Value ExecContext::GetDataInput(int dataIndex)
{
    int seen = 0;
    for (std::size_t i = 0; i < node.inputs.size(); ++i) {
        if (node.inputs[i].type == PinType::Exec) {
            continue;
        }
        if (seen == dataIndex) {
            return engine.GetInputValue(node, static_cast<int>(i), depth);
        }
        ++seen;
    }
    return Value(false);
}

void ExecContext::SetDataOutput(int dataIndex, Value value)
{
    int seen = 0;
    for (std::size_t i = 0; i < node.outputs.size(); ++i) {
        if (node.outputs[i].type == PinType::Exec) {
            continue;
        }
        if (seen == dataIndex) {
            engine.SetOutputValue(node, static_cast<int>(i), std::move(value));
            return;
        }
        ++seen;
    }
}

void ExecContext::RunExecFlow(int execIndex)
{
    execFlowTriggered = true;
    int seen = 0;
    for (std::size_t i = 0; i < node.outputs.size(); ++i) {
        if (node.outputs[i].type != PinType::Exec) {
            continue;
        }
        if (seen == execIndex) {
            engine.RunExecOutputChain(node, static_cast<int>(i));
            return;
        }
        ++seen;
    }
}

void ExecContext::Log(const std::string& message)
{
    engine.Log(message);
}

ExecEngine::ExecEngine(const NodeGraph& graph, LogFn logFn)
    : graph(graph)
    , logFn(std::move(logFn))
{
}

void ExecEngine::Log(const std::string& message)
{
    if (logFn) {
        logFn(message);
    }
}

bool ExecEngine::IsPure(const Node& node)
{
    for (const Pin& pin : node.inputs) {
        if (pin.type == PinType::Exec) {
            return false;
        }
    }
    for (const Pin& pin : node.outputs) {
        if (pin.type == PinType::Exec) {
            return false;
        }
    }
    return true;
}

bool ExecEngine::Run()
{
    executedCount = 0;
    aborted = false;
    outputCache.clear();

    bool anyEntry = false;
    for (const Node& node : graph.GetNodes()) {
        if (std::string(node.nodeClass->GetName()) == "Event Begin") {
            anyEntry = true;
            if (!ExecuteNode(node)) {
                return false;
            }
        }
    }
    if (!anyEntry) {
        Log("error: no Event Begin node in the graph");
        return false;
    }
    return !aborted;
}

PinValueCache ExecEngine::EvaluateDataPreview()
{
    executedCount = 0;
    aborted = false;
    outputCache.clear();

    for (const Node& node : graph.GetNodes()) {
        for (const Pin& outputPin : node.outputs) {
            if (outputPin.type == PinType::Exec || aborted) {
                continue;
            }
            EvaluateOutputPin(outputPin.id, 0);
        }
    }
    return outputCache;
}

bool ExecEngine::ExecuteNode(const Node& node)
{
    if (aborted) {
        return false;
    }
    ++executedCount;
    if (executedCount > MAX_EXECUTED_NODES) {
        aborted = true;
        Log("error: execution budget exceeded (" + std::to_string(MAX_EXECUTED_NODES)
            + " nodes); last node: " + node.nodeClass->GetName()
            + " (id " + std::to_string(node.id) + ")");
        return false;
    }
    RunNodeFunction(node, 0);
    return !aborted;
}

void ExecEngine::RunNodeFunction(const Node& node, int depth)
{
    const std::string& execFnName = node.nodeClass->GetExecFnName();
    const std::string fnName = execFnName.empty() ? node.nodeClass->GetName() : execFnName;
    ExecContext context(*this, node, depth);

    // "wasm:<export>" dispatches into the wasm runtime.
    if (fnName.rfind("wasm:", 0) == 0) {
        std::string error;
        if (!WasmRuntime::Instance().CallNodeFunction(fnName.substr(5), context, error)) {
            aborted = true;
            Log("error: " + error);
            return;
        }
        // A function that never called gau_exec continues down the first
        // exec output by default; an explicit gau_exec (Branch-style
        // control) suppresses this. Pure nodes have no exec outputs and
        // simply end here.
        if (!context.ExecFlowTriggered()) {
            for (int outputIndex = 0; outputIndex < static_cast<int>(node.outputs.size());
                 ++outputIndex) {
                if (node.outputs[static_cast<std::size_t>(outputIndex)].type == PinType::Exec) {
                    context.RunExecOutput(outputIndex);
                    break;
                }
            }
        }
        return;
    }

    const NodeExecFn fn = FindNodeExecFn(fnName.c_str());
    if (fn != nullptr) {
        fn(context);
    } else {
        RunDefaultBehavior(context, node);
    }
}

// Passthrough: each non-exec output takes the same-named input (pulled)
// or the same-named scalar property; then the first exec output chain
// runs. Covers literal nodes and data carriers like Vector3f without a
// native function.
void ExecEngine::RunDefaultBehavior(ExecContext& context, const Node& node)
{
    for (int outputIndex = 0; outputIndex < static_cast<int>(node.outputs.size());
         ++outputIndex) {
        const Pin& outputPin = node.outputs[static_cast<std::size_t>(outputIndex)];
        if (outputPin.type == PinType::Exec) {
            continue;
        }

        bool assigned = false;
        for (int inputIndex = 0; inputIndex < static_cast<int>(node.inputs.size());
             ++inputIndex) {
            const Pin& inputPin = node.inputs[static_cast<std::size_t>(inputIndex)];
            if (inputPin.type != PinType::Exec && inputPin.name == outputPin.name) {
                context.SetOutput(outputIndex, context.GetInput(inputIndex));
                assigned = true;
                break;
            }
        }
        if (assigned) {
            continue;
        }

        const std::vector<PropertyDef>& defs = node.nodeClass->GetProperties();
        for (int propertyIndex = 0; propertyIndex < static_cast<int>(defs.size());
             ++propertyIndex) {
            if (defs[static_cast<std::size_t>(propertyIndex)].name == outputPin.name) {
                context.SetOutput(outputIndex, context.GetProperty(propertyIndex));
                break;
            }
        }
    }

    for (int outputIndex = 0; outputIndex < static_cast<int>(node.outputs.size());
         ++outputIndex) {
        if (node.outputs[static_cast<std::size_t>(outputIndex)].type == PinType::Exec) {
            context.RunExecOutput(outputIndex);
            break;
        }
    }
}

Value ExecEngine::EvaluateOutputPin(PinId outputPinId, int depth)
{
    const Node* node = graph.FindPinOwner(outputPinId);
    if (node == nullptr || aborted) {
        return Value(false);
    }

    if (IsPure(*node)) {
        if (depth > MAX_DATA_DEPTH) {
            aborted = true;
            Log("error: data evaluation depth exceeded (cycle?); node: "
                + std::string(node->nodeClass->GetName()));
            return Value(false);
        }
        // Pure nodes re-evaluate on every request (spec).
        RunNodeFunction(*node, depth + 1);
    }
    return ReadCachedOutput(outputPinId);
}

Value ExecEngine::GetInputValue(const Node& node, int inputIndex, int depth)
{
    if (inputIndex < 0 || inputIndex >= static_cast<int>(node.inputs.size())) {
        return Value(false);
    }
    const Pin& inputPin = node.inputs[static_cast<std::size_t>(inputIndex)];

    const Link* link = graph.FindLinkToInput(inputPin.id);
    if (link != nullptr) {
        return EvaluateOutputPin(link->fromPinId, depth);
    }

    const std::vector<PropertyDef>& defs = node.nodeClass->GetProperties();
    for (std::size_t i = 0; i < defs.size() && i < node.propertyValues.size(); ++i) {
        if (defs[i].container == PropertyContainer::None && defs[i].name == inputPin.name) {
            return node.propertyValues[i].scalar;
        }
    }
    return MakeDefaultValue(inputPin.type);
}

void ExecEngine::SetOutputValue(const Node& node, int outputIndex, Value value)
{
    if (outputIndex < 0 || outputIndex >= static_cast<int>(node.outputs.size())) {
        return;
    }
    const PinId pinId = node.outputs[static_cast<std::size_t>(outputIndex)].id;
    for (std::pair<PinId, Value>& entry : outputCache) {
        if (entry.first == pinId) {
            entry.second = std::move(value);
            return;
        }
    }
    outputCache.emplace_back(pinId, std::move(value));
}

Value ExecEngine::ReadCachedOutput(PinId pinId) const
{
    for (const std::pair<PinId, Value>& entry : outputCache) {
        if (entry.first == pinId) {
            return entry.second;
        }
    }
    return Value(false);
}

void ExecEngine::RunExecOutputChain(const Node& node, int outputIndex)
{
    if (aborted || outputIndex < 0
        || outputIndex >= static_cast<int>(node.outputs.size())) {
        return;
    }
    const Pin& outputPin = node.outputs[static_cast<std::size_t>(outputIndex)];
    if (outputPin.type != PinType::Exec) {
        return;
    }
    const Link* link = graph.FindLinkFromOutput(outputPin.id);
    if (link == nullptr) {
        return;
    }
    const Node* target = graph.FindPinOwner(link->toPinId);
    if (target != nullptr) {
        ExecuteNode(*target);
    }
}
