// Collapse/expand implementation for functions.

#include "FunctionOps.h"

#include "FunctionNodes.h"

#include <string>
#include <utility>
#include <vector>

namespace gau {

namespace {

bool Contains(const std::vector<NodeId>& set, NodeId id)
{
    for (NodeId n : set) {
        if (n == id) {
            return true;
        }
    }
    return false;
}

bool ContainsName(const std::vector<std::string>& used, const std::string& name)
{
    for (const std::string& n : used) {
        if (n == name) {
            return true;
        }
    }
    return false;
}

std::string UniqueName(std::vector<std::string>& used, std::string base)
{
    if (base.empty()) {
        base = "p";
    }
    std::string name = base;
    int k = 1;
    while (ContainsName(used, name)) {
        name = base + std::to_string(k++);
    }
    used.push_back(name);
    return name;
}

bool IsExec(const Graph& g, const TypeRegistry& types, PinId pin)
{
    const Pin* p = g.FindPin(pin);
    if (p == nullptr) {
        return false;
    }
    const TypeDesc* d = types.Resolve(p->type);
    return d != nullptr && d->tag == TypeTag::Exec;
}

// Maps a main-graph pin to the cloned body pin (or vice versa) via a flat
// pin-id pairing built during cloning.
PinId MapPin(const std::vector<std::pair<PinId, PinId>>& pinMap, PinId src)
{
    for (const auto& e : pinMap) {
        if (e.first == src) {
            return e.second;
        }
    }
    return INVALID_ID;
}

// Copies a node (class instance + property values) into `dst`, recording the
// source->dest pin pairing. Returns the new node id.
NodeId CloneNode(const Graph& src, Graph& dst, const NodeClassRegistry& classes,
                 const Node& node, std::vector<std::pair<PinId, PinId>>& pinMap)
{
    const NodeClass* cls = classes.Find(node.className);
    if (cls == nullptr) {
        return INVALID_ID;
    }
    const NodeId newId = dst.AddNode(*cls, node.x, node.y);
    Node* dstNode = dst.FindNode(newId);
    if (dstNode == nullptr) {
        return INVALID_ID;
    }
    for (std::size_t i = 0; i < node.inputs.size() && i < dstNode->inputs.size(); ++i) {
        pinMap.emplace_back(node.inputs[i].id, dstNode->inputs[i].id);
    }
    for (std::size_t i = 0; i < node.outputs.size() && i < dstNode->outputs.size(); ++i) {
        pinMap.emplace_back(node.outputs[i].id, dstNode->outputs[i].id);
    }
    dstNode->properties = node.properties;
    return newId;
}

} // namespace

NodeId CollapseSelection(Graph& main, TypeRegistry& types, NodeClassRegistry& classes,
                         BuiltinRegistry& builtins, FunctionRegistry& functions,
                         const std::vector<NodeId>& selection, const std::string& name)
{
    if (selection.empty() || classes.Find(name) != nullptr
        || functions.Find(name) != nullptr) {
        return INVALID_ID;
    }
    for (NodeId id : selection) {
        const Node* n = main.FindNode(id);
        if (n == nullptr || classes.Find(n->className) == nullptr) {
            return INVALID_ID;
        }
    }

    // Classify every link relative to the selection.
    struct DataConn { PinId source; PinId sink; };
    std::vector<DataConn> internalLinks;
    std::vector<DataConn> boundaryIn;  // extSource -> intSink (data)
    std::vector<DataConn> boundaryOut; // intSource -> extSink (data)
    std::vector<DataConn> execIn;      // extSource -> intSink (exec)
    std::vector<DataConn> execOut;     // intSource -> extSink (exec)

    for (const Link& link : main.Links()) {
        const Node* fromN = main.FindPinOwner(link.fromPin);
        const Node* toN = main.FindPinOwner(link.toPin);
        if (fromN == nullptr || toN == nullptr) {
            continue;
        }
        const bool fromSel = Contains(selection, fromN->id);
        const bool toSel = Contains(selection, toN->id);
        const bool exec = IsExec(main, types, link.fromPin);
        const DataConn conn{link.fromPin, link.toPin};
        if (fromSel && toSel) {
            internalLinks.push_back(conn);
        } else if (!fromSel && toSel) {
            (exec ? execIn : boundaryIn).push_back(conn);
        } else if (fromSel && !toSel) {
            (exec ? execOut : boundaryOut).push_back(conn);
        }
    }

    const bool hasExec = !execIn.empty() || !execOut.empty();

    // Input params: one per distinct external source pin (data). Each may
    // feed several internal consumers.
    std::vector<PinId> inputSources;
    std::vector<std::vector<PinId>> inputConsumers;
    for (const DataConn& c : boundaryIn) {
        int idx = -1;
        for (std::size_t j = 0; j < inputSources.size(); ++j) {
            if (inputSources[j] == c.source) {
                idx = static_cast<int>(j);
                break;
            }
        }
        if (idx < 0) {
            idx = static_cast<int>(inputSources.size());
            inputSources.push_back(c.source);
            inputConsumers.emplace_back();
        }
        inputConsumers[static_cast<std::size_t>(idx)].push_back(c.sink);
    }

    // Output params: one per distinct internal source pin (data). Each may
    // drive several external consumers.
    std::vector<PinId> outputSources;
    std::vector<std::vector<PinId>> outputConsumers;
    for (const DataConn& c : boundaryOut) {
        int idx = -1;
        for (std::size_t m = 0; m < outputSources.size(); ++m) {
            if (outputSources[m] == c.source) {
                idx = static_cast<int>(m);
                break;
            }
        }
        if (idx < 0) {
            idx = static_cast<int>(outputSources.size());
            outputSources.push_back(c.source);
            outputConsumers.emplace_back();
        }
        outputConsumers[static_cast<std::size_t>(idx)].push_back(c.sink);
    }

    // Build the function definition and generate its node classes/behaviors.
    FunctionDef* def = functions.Create(types, name);
    if (def == nullptr) {
        return INVALID_ID;
    }
    std::vector<std::string> inNames;
    for (PinId src : inputSources) {
        const Pin* p = main.FindPin(src);
        def->inputs.push_back(FunctionParam{UniqueName(inNames, p ? p->name : ""),
                                            p ? p->type : INVALID_TYPE});
    }
    std::vector<std::string> outNames;
    for (PinId src : outputSources) {
        const Pin* p = main.FindPin(src);
        def->outputs.push_back(FunctionParam{UniqueName(outNames, p ? p->name : ""),
                                             p ? p->type : INVALID_TYPE});
    }
    def->hasExec = hasExec;
    RegisterFunctionNodes(classes, builtins, types, *def);

    // Clone the selected nodes and their internal links into the body.
    Graph& body = *def->body;
    std::vector<std::pair<PinId, PinId>> toBody; // main pin -> body pin
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (NodeId id : selection) {
        const Node* n = main.FindNode(id);
        CloneNode(main, body, classes, *n, toBody);
        sumX += n->x;
        sumY += n->y;
    }
    for (const DataConn& c : internalLinks) {
        body.AddLink(MapPin(toBody, c.source), MapPin(toBody, c.sink));
    }

    // Entry/Return marshalling nodes.
    const int off = hasExec ? 1 : 0;
    const NodeId entry = body.AddNode(*classes.Find(name + " In"), 0, 0);
    const NodeId ret = body.AddNode(*classes.Find(name + " Out"), 0, 0);
    def->entryNode = entry;
    def->returnNode = ret;

    std::vector<PinId> entryOut;
    for (const Pin& p : body.FindNode(entry)->outputs) {
        entryOut.push_back(p.id);
    }
    std::vector<PinId> retIn;
    for (const Pin& p : body.FindNode(ret)->inputs) {
        retIn.push_back(p.id);
    }

    for (std::size_t j = 0; j < inputConsumers.size(); ++j) {
        const PinId entryPin = entryOut[static_cast<std::size_t>(off) + j];
        for (PinId consumer : inputConsumers[j]) {
            body.AddLink(entryPin, MapPin(toBody, consumer));
        }
    }
    for (std::size_t m = 0; m < outputSources.size(); ++m) {
        body.AddLink(MapPin(toBody, outputSources[m]), retIn[static_cast<std::size_t>(off) + m]);
    }
    if (hasExec) {
        for (const DataConn& c : execIn) {
            body.AddLink(entryOut[0], MapPin(toBody, c.sink));
        }
        for (const DataConn& c : execOut) {
            body.AddLink(MapPin(toBody, c.source), retIn[0]);
        }
    }

    // Place the Call node at the selection centroid and rewire boundaries.
    const float cx = sumX / static_cast<float>(selection.size());
    const float cy = sumY / static_cast<float>(selection.size());
    const NodeId call = main.AddNode(*classes.Find(name), cx, cy);
    std::vector<PinId> callIn;
    std::vector<PinId> callOut;
    for (const Pin& p : main.FindNode(call)->inputs) {
        callIn.push_back(p.id);
    }
    for (const Pin& p : main.FindNode(call)->outputs) {
        callOut.push_back(p.id);
    }

    for (std::size_t j = 0; j < inputSources.size(); ++j) {
        main.AddLink(inputSources[j], callIn[static_cast<std::size_t>(off) + j]);
    }
    for (std::size_t m = 0; m < outputConsumers.size(); ++m) {
        const PinId callPin = callOut[static_cast<std::size_t>(off) + m];
        for (PinId consumer : outputConsumers[m]) {
            main.AddLink(callPin, consumer);
        }
    }
    if (hasExec) {
        for (const DataConn& c : execIn) {
            main.AddLink(c.source, callIn[0]);
        }
        for (const DataConn& c : execOut) {
            main.AddLink(callOut[0], c.sink);
        }
    }

    for (NodeId id : selection) {
        main.RemoveNode(id);
    }
    return call;
}

bool ExpandCall(Graph& main, TypeRegistry& types, NodeClassRegistry& classes,
                const FunctionRegistry& functions, NodeId callNode)
{
    const Node* call = main.FindNode(callNode);
    if (call == nullptr) {
        return false;
    }
    const FunctionDef* def = functions.Find(call->className);
    if (def == nullptr) {
        return false;
    }
    const Graph& body = *def->body;
    const int off = def->hasExec ? 1 : 0;

    // Snapshot the call's boundary: what feeds each input pin and what each
    // output pin drives, indexed by pin position on the call node.
    std::vector<PinId> callIn;
    std::vector<PinId> callOut;
    for (const Pin& p : call->inputs) {
        callIn.push_back(p.id);
    }
    for (const Pin& p : call->outputs) {
        callOut.push_back(p.id);
    }
    std::vector<PinId> inSource(callIn.size(), INVALID_ID);       // ext -> callIn[i]
    std::vector<std::vector<PinId>> outSinks(callOut.size());     // callOut[i] -> ext
    for (const Link& link : main.Links()) {
        for (std::size_t i = 0; i < callIn.size(); ++i) {
            if (link.toPin == callIn[i]) {
                inSource[i] = link.fromPin;
            }
        }
        for (std::size_t i = 0; i < callOut.size(); ++i) {
            if (link.fromPin == callOut[i]) {
                outSinks[i].push_back(link.toPin);
            }
        }
    }

    // Clone the body interior (everything but Entry/Return) into main.
    std::vector<std::pair<PinId, PinId>> toMain; // body pin -> main pin
    for (const Node& n : body.Nodes()) {
        if (n.id == def->entryNode || n.id == def->returnNode) {
            continue;
        }
        CloneNode(body, main, classes, n, toMain);
    }
    for (const Link& link : body.Links()) {
        const Node* fromN = body.FindPinOwner(link.fromPin);
        const Node* toN = body.FindPinOwner(link.toPin);
        if (fromN == nullptr || toN == nullptr) {
            continue;
        }
        const bool touchesMarshal = fromN->id == def->entryNode || fromN->id == def->returnNode
                                    || toN->id == def->entryNode || toN->id == def->returnNode;
        if (touchesMarshal) {
            continue;
        }
        main.AddLink(MapPin(toMain, link.fromPin), MapPin(toMain, link.toPin));
    }

    // Rewire Entry outputs: each Entry output pin feeds interior consumers;
    // hook the caller's source for that param to those consumers instead.
    const Node* entry = body.FindNode(def->entryNode);
    const Node* ret = body.FindNode(def->returnNode);
    for (std::size_t o = 0; o < entry->outputs.size(); ++o) {
        const PinId entryPin = entry->outputs[o].id;
        // param index p maps to call input pin off + p (exec output is o==0).
        const bool isExec = def->hasExec && o == 0;
        const std::size_t callInIdx = isExec ? 0 : (off + (o - static_cast<std::size_t>(off)));
        const PinId src = callInIdx < inSource.size() ? inSource[callInIdx] : INVALID_ID;
        if (src == INVALID_ID) {
            continue;
        }
        for (const Link& link : body.Links()) {
            if (link.fromPin != entryPin) {
                continue;
            }
            main.AddLink(src, MapPin(toMain, link.toPin));
        }
    }

    // Rewire Return inputs: each Return input is driven by an interior
    // producer; hook that producer to the caller's downstream sinks.
    for (std::size_t in = 0; in < ret->inputs.size(); ++in) {
        const PinId retPin = ret->inputs[in].id;
        const bool isExec = def->hasExec && in == 0;
        const std::size_t callOutIdx = isExec ? 0 : (off + (in - static_cast<std::size_t>(off)));
        if (callOutIdx >= outSinks.size()) {
            continue;
        }
        for (const Link& link : body.Links()) {
            if (link.toPin != retPin) {
                continue;
            }
            const PinId producer = MapPin(toMain, link.fromPin);
            for (PinId sink : outSinks[callOutIdx]) {
                main.AddLink(producer, sink);
            }
        }
    }

    main.RemoveNode(callNode);
    return true;
}

} // namespace gau
