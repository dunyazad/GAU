// v2 execution runtime implementation.

#include "Runtime.h"

#include "WasmHost.h"

#include "core/TypeRegistry.h"

#include <string>
#include <utility>

namespace gau {

// Bridges a NodeEval to the wasm host. The host ABI is scalar-only, so a
// struct-typed pin is flattened into its leaf scalars over a flat index space:
// the wasm side reads/writes consecutive indices (matching the generated
// gau_read_/gau_write_ helpers), and the runtime reassembles struct outputs
// afterwards. This is what lets custom struct types flow through wasm nodes on
// a single pin instead of being decomposed into separate component pins.
namespace {

// Appends the scalar leaves of a value in field order (recurses into structs).
void FlattenLeaves(const Value& value, std::vector<Value>& out)
{
    if (const StructVal* sv = std::get_if<StructVal>(&value.data)) {
        for (const Value& field : sv->fields) {
            FlattenLeaves(field, out);
        }
        return;
    }
    out.push_back(value);
}

// Number of scalar leaves a type occupies in the flat index space.
int LeafCount(const TypeRegistry& types, TypeId id)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc != nullptr && desc->tag == TypeTag::Struct) {
        const StructDef* def = types.FindStruct(desc->name);
        int count = 0;
        if (def != nullptr) {
            for (const StructField& field : def->fields) {
                count += LeafCount(types, field.type);
            }
        }
        return count;
    }
    return 1;
}

// Rebuilds a value of the given type by consuming leaves from `leaves` starting
// at `cursor` (recurses into structs).
Value AssembleLeaves(const TypeRegistry& types, TypeId id, const std::vector<Value>& leaves,
                     std::size_t& cursor)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc != nullptr && desc->tag == TypeTag::Struct) {
        const StructDef* def = types.FindStruct(desc->name);
        StructVal sv;
        if (def != nullptr) {
            for (const StructField& field : def->fields) {
                sv.fields.push_back(AssembleLeaves(types, field.type, leaves, cursor));
            }
        }
        return Value(ValueData(std::move(sv)));
    }
    Value leaf = (cursor < leaves.size()) ? leaves[cursor] : Value::None();
    ++cursor;
    return leaf;
}

class FlatWasmContext : public WasmNodeContext
{
public:
    // Wasm ABI index spaces keep data and exec pins separate: input/output
    // leaf indices span only non-exec pins (structs flatten to one leaf per
    // field), and RunExec addresses the nth exec output pin. Exec pins get
    // offset -1 so no leaf index ever resolves to them.
    FlatWasmContext(NodeEval& eval, const TypeRegistry& types) : eval(eval), types(types)
    {
        int offset = 0;
        for (const Pin& pin : eval.node->inputs) {
            if (IsExecPin(pin)) {
                inputLeafOffset.push_back(-1);
            } else {
                inputLeafOffset.push_back(offset);
                offset += LeafCount(types, pin.type);
            }
        }
        inputCache.assign(eval.node->inputs.size(), {});
        inputCached.assign(eval.node->inputs.size(), false);

        offset = 0;
        for (const Pin& pin : eval.node->outputs) {
            if (IsExecPin(pin)) {
                outputLeafOffset.push_back(-1);
            } else {
                outputLeafOffset.push_back(offset);
                offset += LeafCount(types, pin.type);
            }
        }
        totalOutputLeaves = offset;
        outputLeaves.assign(static_cast<std::size_t>(offset), Value::None());
        outputWritten.assign(static_cast<std::size_t>(offset), false);
    }

    Value Input(int index) override
    {
        const std::vector<Pin>& pins = eval.node->inputs;
        for (std::size_t i = 0; i < pins.size(); ++i) {
            const int base = inputLeafOffset[i];
            if (base < 0) {
                continue;
            }
            const int next = base + LeafCount(types, pins[i].type);
            if (index < base || index >= next) {
                continue;
            }
            if (!inputCached[i]) {
                FlattenLeaves(eval.In(static_cast<int>(i)), inputCache[i]);
                inputCached[i] = true;
            }
            const std::size_t leaf = static_cast<std::size_t>(index - base);
            return leaf < inputCache[i].size() ? inputCache[i][leaf] : Value::None();
        }
        return Value::None();
    }

    Value Property(int index) override { return eval.Prop(index); }

    void SetOutput(int index, Value value) override
    {
        if (index >= 0 && index < totalOutputLeaves) {
            outputLeaves[static_cast<std::size_t>(index)] = std::move(value);
            outputWritten[static_cast<std::size_t>(index)] = true;
        }
    }

    void RunExec(int index) override
    {
        const std::vector<Pin>& pins = eval.node->outputs;
        int seen = 0;
        for (std::size_t i = 0; i < pins.size(); ++i) {
            if (!IsExecPin(pins[i])) {
                continue;
            }
            if (seen == index) {
                eval.Then(static_cast<int>(i));
                return;
            }
            ++seen;
        }
    }

    void Log(const std::string& message) override { eval.rt->Log(message); }

    // Reassembles struct/scalar output pins from the leaves the wasm wrote.
    // Pins the wasm never wrote are left untouched (default pull behavior).
    void Finalize()
    {
        const std::vector<Pin>& pins = eval.node->outputs;
        for (std::size_t i = 0; i < pins.size(); ++i) {
            const int base = outputLeafOffset[i];
            if (base < 0) {
                continue;
            }
            const int count = LeafCount(types, pins[i].type);
            bool anyWritten = false;
            for (int k = 0; k < count; ++k) {
                if (base + k < totalOutputLeaves
                    && outputWritten[static_cast<std::size_t>(base + k)]) {
                    anyWritten = true;
                    break;
                }
            }
            if (!anyWritten) {
                continue;
            }
            std::size_t cursor = static_cast<std::size_t>(base);
            eval.Out(static_cast<int>(i), AssembleLeaves(types, pins[i].type, outputLeaves, cursor));
        }
    }

private:
    bool IsExecPin(const Pin& pin) const
    {
        const TypeDesc* desc = types.Resolve(pin.type);
        return desc != nullptr && desc->tag == TypeTag::Exec;
    }

    NodeEval& eval;
    const TypeRegistry& types;
    std::vector<int> inputLeafOffset;
    std::vector<std::vector<Value>> inputCache;
    std::vector<bool> inputCached;
    std::vector<int> outputLeafOffset;
    std::vector<Value> outputLeaves;
    std::vector<bool> outputWritten;
    int totalOutputLeaves = 0;
};

} // namespace

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

void Runtime::SetVariable(const std::string& name, Value value)
{
    for (auto& entry : variables) {
        if (entry.first == name) {
            entry.second = std::move(value);
            return;
        }
    }
    variables.emplace_back(name, std::move(value));
}

Value Runtime::GetVariable(const std::string& name) const
{
    for (const auto& entry : variables) {
        if (entry.first == name) {
            return entry.second;
        }
    }
    return Value::None();
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
    // A class bound to "wasm:<export>" dispatches into the wasm host; anything
    // else runs its native builtin. The wasm export talks to this node through
    // the NodeEval bridge.
    const NodeClass* cls = ClassOf(node);
    if (cls != nullptr && cls->execFn.rfind("wasm:", 0) == 0) {
        NodeEval eval;
        eval.rt = this;
        eval.node = &node;
        FlatWasmContext context(eval, *types);
        std::string error;
        if (WasmHost::Instance().CallNodeFunction(cls->execFn.substr(5), context, error)) {
            context.Finalize();
        } else {
            Log("wasm error: " + error);
        }
    } else {
        const NodeFn* fn = builtins->Find(node.className);
        if (fn != nullptr) {
            NodeEval eval;
            eval.rt = this;
            eval.node = &node;
            (*fn)(eval);
        }
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
    variables.clear();
    pcNode = entryNode;
    chosenExec = INVALID_ID;
    ignoreBreakOnce = false;
    lastError = RunError{};
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
        lastError = RunError{RunErrorKind::NodeNotFound, pcNode,
                             "exec reached a missing node id " + std::to_string(pcNode)};
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
    if (state == RunState::Running) {
        lastError = RunError{RunErrorKind::StepLimitExceeded, pcNode,
                             "step limit (" + std::to_string(maxSteps)
                                 + ") exceeded at node " + std::to_string(pcNode)};
        state = RunState::Error;
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
