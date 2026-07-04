// v2 execution runtime implementation.

#include "Runtime.h"

#include "core/TypeRegistry.h"

#include <utility>

namespace gau {

void BuiltinRegistry::Register(std::string name, NodeFn fn)
{
    for (auto& entry : fns) {
        if (entry.first == name) {
            entry.second = std::move(fn);
            return;
        }
    }
    fns.emplace_back(std::move(name), std::move(fn));
}

const NodeFn* BuiltinRegistry::Find(const std::string& name) const
{
    for (const auto& entry : fns) {
        if (entry.first == name) {
            return &entry.second;
        }
    }
    return nullptr;
}

Runtime::Runtime(const Graph& graph, const TypeRegistry& types, const NodeClassRegistry& classes,
                 const BuiltinRegistry& builtins, LogFn log)
    : graph(&graph)
    , types(&types)
    , classes(&classes)
    , builtins(&builtins)
    , log(std::move(log))
{
}

void Runtime::AddBreakpoint(NodeId nodeId)
{
    for (NodeId id : breakpoints) {
        if (id == nodeId) {
            return;
        }
    }
    breakpoints.push_back(nodeId);
}

void Runtime::ClearBreakpoints()
{
    breakpoints.clear();
}

void Runtime::Log(const std::string& message) const
{
    if (log) {
        log(message);
    }
}

const NodeClass* Runtime::ClassOf(const Node& node) const
{
    return classes->Find(node.className);
}

bool Runtime::IsExecPin(const Pin& pin) const
{
    const TypeDesc* desc = types->Resolve(pin.type);
    return desc != nullptr && desc->tag == TypeTag::Exec;
}

bool Runtime::IsPureClass(const NodeClass& cls) const
{
    for (const PinDef& pinDef : cls.pins) {
        const TypeDesc* desc = types->Resolve(pinDef.type);
        if (desc != nullptr && desc->tag == TypeTag::Exec) {
            return false;
        }
    }
    return true;
}

Value Runtime::CachedOutput(PinId outputPinId) const
{
    for (const auto& entry : outputCache) {
        if (entry.first == outputPinId) {
            return entry.second;
        }
    }
    for (const auto& entry : execOutputCache) {
        if (entry.first == outputPinId) {
            return entry.second;
        }
    }
    return Value::None();
}

void Runtime::LatchExecOutputs(const Node& node)
{
    for (const Pin& pin : node.outputs) {
        if (IsExecPin(pin)) {
            continue;
        }
        for (const auto& produced : outputCache) {
            if (produced.first != pin.id) {
                continue;
            }
            bool found = false;
            for (auto& latched : execOutputCache) {
                if (latched.first == pin.id) {
                    latched.second = produced.second;
                    found = true;
                    break;
                }
            }
            if (!found) {
                execOutputCache.emplace_back(pin.id, produced.second);
            }
            break;
        }
    }
}

void Runtime::SetOutput(PinId outputPinId, Value value)
{
    for (auto& entry : outputCache) {
        if (entry.first == outputPinId) {
            entry.second = std::move(value);
            return;
        }
    }
    outputCache.emplace_back(outputPinId, std::move(value));
}

void Runtime::ChooseNext(PinId execOutputPinId)
{
    chosenExec = execOutputPinId;
}

const Value& Runtime::Property(const Node& node, int index) const
{
    static const Value none;
    if (index < 0 || index >= static_cast<int>(node.properties.size())) {
        return none;
    }
    return node.properties[static_cast<std::size_t>(index)];
}

void Runtime::SetParamsIn(std::vector<Value> params)
{
    paramsIn = std::move(params);
}

Value Runtime::ParamIn(int index) const
{
    if (index < 0 || index >= static_cast<int>(paramsIn.size())) {
        return Value::None();
    }
    return paramsIn[static_cast<std::size_t>(index)];
}

void Runtime::SetResult(int index, Value value)
{
    if (index < 0) {
        return;
    }
    if (index >= static_cast<int>(resultsOut.size())) {
        resultsOut.resize(static_cast<std::size_t>(index) + 1);
    }
    resultsOut[static_cast<std::size_t>(index)] = std::move(value);
}

Value Runtime::ResultOut(int index) const
{
    if (index < 0 || index >= static_cast<int>(resultsOut.size())) {
        return Value::None();
    }
    return resultsOut[static_cast<std::size_t>(index)];
}

void Runtime::EvalNode(NodeId nodeId)
{
    const Node* node = graph->FindNode(nodeId);
    if (node != nullptr) {
        EvaluateNode(*node);
    }
}

Value Runtime::EvalInputPin(PinId inputPinId)
{
    const Pin* inputPin = graph->FindPin(inputPinId);
    if (inputPin == nullptr) {
        return Value::None();
    }
    // Find the link feeding this input pin.
    PinId sourcePin = INVALID_ID;
    for (const Link& link : graph->Links()) {
        if (link.toPin == inputPinId) {
            sourcePin = link.fromPin;
            break;
        }
    }
    if (sourcePin == INVALID_ID) {
        return types->MakeDefault(inputPin->type);
    }

    const Node* sourceNode = graph->FindPinOwner(sourcePin);
    if (sourceNode != nullptr) {
        const NodeClass* cls = ClassOf(*sourceNode);
        if (cls != nullptr && IsPureClass(*cls)) {
            EvaluateNode(*sourceNode);
        }
    }
    const Pin* srcPinPtr = graph->FindPin(sourcePin);
    Value cached = CachedOutput(sourcePin);
    if (!std::holds_alternative<std::monostate>(cached.data)) {
        return cached;
    }
    return srcPinPtr != nullptr ? types->MakeDefault(srcPinPtr->type) : Value::None();
}

void Runtime::EvaluateNode(const Node& node)
{
    for (NodeId id : evalStack) {
        if (id == node.id) {
            return; // cycle guard for pure data evaluation
        }
    }
    evalStack.push_back(node.id);
    const NodeFn* fn = builtins->Find(node.className);
    if (fn != nullptr) {
        NodeEval eval;
        eval.rt = this;
        eval.node = &node;
        (*fn)(eval);
    }
    evalStack.pop_back();
}

PinId Runtime::DefaultNextExec(const Node& node) const
{
    for (const Pin& pin : node.outputs) {
        if (IsExecPin(pin)) {
            return pin.id;
        }
    }
    return INVALID_ID;
}

NodeId Runtime::FollowExec(PinId execOutputPinId) const
{
    for (const Link& link : graph->Links()) {
        if (link.fromPin == execOutputPinId) {
            const Node* owner = graph->FindPinOwner(link.toPin);
            return owner != nullptr ? owner->id : INVALID_ID;
        }
    }
    return INVALID_ID;
}

void Runtime::Start(NodeId entryNode)
{
    outputCache.clear();
    execOutputCache.clear();
    evalStack.clear();
    pcNode = entryNode;
    chosenExec = INVALID_ID;
    ignoreBreakOnce = false;
    state = (entryNode != INVALID_ID) ? RunState::Running : RunState::Done;
}

RunState Runtime::Step()
{
    if (state != RunState::Running) {
        return state;
    }
    if (pcNode == INVALID_ID) {
        state = RunState::Done;
        return state;
    }
    if (!ignoreBreakOnce) {
        for (NodeId id : breakpoints) {
            if (id == pcNode) {
                state = RunState::Paused;
                return state;
            }
        }
    }
    ignoreBreakOnce = false;

    const Node* node = graph->FindNode(pcNode);
    if (node == nullptr) {
        state = RunState::Error;
        return state;
    }

    outputCache.clear();
    chosenExec = INVALID_ID;
    EvaluateNode(*node);
    LatchExecOutputs(*node);

    PinId nextExec = (chosenExec != INVALID_ID) ? chosenExec : DefaultNextExec(*node);
    pcNode = (nextExec != INVALID_ID) ? FollowExec(nextExec) : INVALID_ID;
    if (pcNode == INVALID_ID) {
        state = RunState::Done;
    }
    return state;
}

void Runtime::Continue()
{
    if (state == RunState::Paused) {
        ignoreBreakOnce = true;
        state = RunState::Running;
    }
}

bool Runtime::Run(int maxSteps)
{
    int steps = 0;
    while (state == RunState::Running && steps < maxSteps) {
        Step();
        ++steps;
    }
    return state == RunState::Done;
}

Value Runtime::EvalPin(PinId pinId)
{
    const Pin* pin = graph->FindPin(pinId);
    if (pin == nullptr) {
        return Value::None();
    }
    if (pin->direction == PinDirection::Input) {
        return EvalInputPin(pinId);
    }
    const Node* owner = graph->FindPinOwner(pinId);
    if (owner != nullptr) {
        const NodeClass* cls = ClassOf(*owner);
        if (cls != nullptr && IsPureClass(*cls)) {
            EvaluateNode(*owner);
        }
    }
    Value cached = CachedOutput(pinId);
    if (!std::holds_alternative<std::monostate>(cached.data)) {
        return cached;
    }
    return types->MakeDefault(pin->type);
}

Value NodeEval::In(int inputIndex) const
{
    if (node == nullptr || inputIndex < 0
        || inputIndex >= static_cast<int>(node->inputs.size())) {
        return Value::None();
    }
    return rt->EvalInputPin(node->inputs[static_cast<std::size_t>(inputIndex)].id);
}

const Value& NodeEval::Prop(int propertyIndex) const
{
    return rt->Property(*node, propertyIndex);
}

void NodeEval::Out(int outputIndex, Value value) const
{
    if (node == nullptr || outputIndex < 0
        || outputIndex >= static_cast<int>(node->outputs.size())) {
        return;
    }
    rt->SetOutput(node->outputs[static_cast<std::size_t>(outputIndex)].id, std::move(value));
}

void NodeEval::Then(int outputIndex) const
{
    if (node == nullptr || outputIndex < 0
        || outputIndex >= static_cast<int>(node->outputs.size())) {
        return;
    }
    rt->ChooseNext(node->outputs[static_cast<std::size_t>(outputIndex)].id);
}

} // namespace gau
