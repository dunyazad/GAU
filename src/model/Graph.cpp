// v2 graph operations: node instantiation, connection rules, links.

#include "Graph.h"

#include "core/TypeRegistry.h"

#include <utility>

namespace gau {

NodeId Graph::AddNode(const NodeClass& nodeClass, float x, float y)
{
    Node node;
    node.id = nextNodeId++;
    node.guid = "n" + std::to_string(node.id);
    node.className = nodeClass.name;
    node.x = x;
    node.y = y;
    for (const PinDef& pinDef : nodeClass.pins) {
        Pin pin;
        pin.id = nextPinId++;
        pin.node = node.id;
        pin.direction = pinDef.direction;
        pin.type = pinDef.type;
        pin.name = pinDef.name;
        if (pinDef.direction == PinDirection::Input) {
            node.inputs.push_back(std::move(pin));
        } else {
            node.outputs.push_back(std::move(pin));
        }
    }
    for (const PropertyDef& propertyDef : nodeClass.properties) {
        node.properties.push_back(propertyDef.defaultValue);
    }
    nodes.push_back(std::move(node));
    return nodes.back().id;
}

PinId Graph::AppendPin(NodeId nodeId, PinDirection direction, TypeId type,
                       const std::string& name)
{
    Node* node = FindNode(nodeId);
    if (node == nullptr) {
        return INVALID_ID;
    }
    Pin pin;
    pin.id = nextPinId++;
    pin.node = nodeId;
    pin.direction = direction;
    pin.type = type;
    pin.name = name;
    const PinId id = pin.id;
    if (direction == PinDirection::Input) {
        node->inputs.push_back(std::move(pin));
    } else {
        node->outputs.push_back(std::move(pin));
    }
    return id;
}

bool Graph::RemovePin(NodeId nodeId, PinId pinId)
{
    Node* node = FindNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    for (std::size_t i = links.size(); i-- > 0;) {
        if (links[i].fromPin == pinId || links[i].toPin == pinId) {
            links.erase(links.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    for (std::size_t i = 0; i < node->inputs.size(); ++i) {
        if (node->inputs[i].id == pinId) {
            node->inputs.erase(node->inputs.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    for (std::size_t i = 0; i < node->outputs.size(); ++i) {
        if (node->outputs[i].id == pinId) {
            node->outputs.erase(node->outputs.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

bool Graph::RemoveNode(NodeId nodeId)
{
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].id == nodeId) {
            RemoveLinksTouchingNode(nodeId);
            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

Node* Graph::FindNode(NodeId nodeId)
{
    for (Node& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

const Node* Graph::FindNode(NodeId nodeId) const
{
    for (const Node& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

const Pin* Graph::FindPin(PinId pinId) const
{
    for (const Node& node : nodes) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) {
                return &pin;
            }
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) {
                return &pin;
            }
        }
    }
    return nullptr;
}

const Node* Graph::FindPinOwner(PinId pinId) const
{
    for (const Node& node : nodes) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) {
                return &node;
            }
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) {
                return &node;
            }
        }
    }
    return nullptr;
}

bool Graph::IsExecPin(PinId pinId) const
{
    const Pin* pin = FindPin(pinId);
    if (pin == nullptr) {
        return false;
    }
    const TypeDesc* desc = types->Resolve(pin->type);
    return desc != nullptr && desc->tag == TypeTag::Exec;
}

bool Graph::Normalize(PinId a, PinId b, PinId& outOutput, PinId& outInput) const
{
    const Pin* pa = FindPin(a);
    const Pin* pb = FindPin(b);
    if (pa == nullptr || pb == nullptr || pa->direction == pb->direction) {
        return false;
    }
    if (pa->direction == PinDirection::Output) {
        outOutput = a;
        outInput = b;
    } else {
        outOutput = b;
        outInput = a;
    }
    return true;
}

bool Graph::CanConnect(PinId a, PinId b) const
{
    const Pin* pa = FindPin(a);
    const Pin* pb = FindPin(b);
    if (pa == nullptr || pb == nullptr) {
        return false;
    }
    if (pa->node == pb->node) {
        return false;
    }
    if (pa->direction == pb->direction) {
        return false;
    }
    if (pa->type != pb->type) {
        return false;
    }
    // Exec links must not create a cycle.
    if (IsExecPin(a)) {
        PinId output = INVALID_ID;
        PinId input = INVALID_ID;
        if (!Normalize(a, b, output, input)) {
            return false;
        }
        const Node* outNode = FindPinOwner(output);
        const Node* inNode = FindPinOwner(input);
        if (outNode == nullptr || inNode == nullptr) {
            return false;
        }
        if (ExecPathExists(inNode->id, outNode->id)) {
            return false;
        }
    }
    return true;
}

LinkId Graph::AddLink(PinId outputPin, PinId inputPin)
{
    if (!CanConnect(outputPin, inputPin)) {
        return INVALID_ID;
    }
    PinId output = INVALID_ID;
    PinId input = INVALID_ID;
    if (!Normalize(outputPin, inputPin, output, input)) {
        return INVALID_ID;
    }
    // A data input and an exec output hold at most one link: replace.
    const bool inputIsExec = IsExecPin(input);
    if (!inputIsExec) {
        for (std::size_t i = 0; i < links.size(); ++i) {
            if (links[i].toPin == input) {
                links.erase(links.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
    }
    Link link;
    link.id = nextLinkId++;
    link.fromPin = output;
    link.toPin = input;
    links.push_back(std::move(link));
    return links.back().id;
}

bool Graph::RemoveLink(LinkId linkId)
{
    for (std::size_t i = 0; i < links.size(); ++i) {
        if (links[i].id == linkId) {
            links.erase(links.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

void Graph::RemoveLinksTouchingNode(NodeId nodeId)
{
    for (std::size_t i = links.size(); i-- > 0;) {
        const Node* from = FindPinOwner(links[i].fromPin);
        const Node* to = FindPinOwner(links[i].toPin);
        if ((from != nullptr && from->id == nodeId) || (to != nullptr && to->id == nodeId)) {
            links.erase(links.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

bool Graph::ExecPathExists(NodeId fromNode, NodeId toNode) const
{
    if (fromNode == toNode) {
        return true;
    }
    std::vector<NodeId> stack{fromNode};
    std::vector<NodeId> visited;
    while (!stack.empty()) {
        const NodeId current = stack.back();
        stack.pop_back();
        bool seen = false;
        for (NodeId v : visited) {
            if (v == current) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }
        visited.push_back(current);
        for (const Link& link : links) {
            if (!IsExecPin(link.fromPin)) {
                continue;
            }
            const Node* from = FindPinOwner(link.fromPin);
            const Node* to = FindPinOwner(link.toPin);
            if (from == nullptr || to == nullptr || from->id != current) {
                continue;
            }
            if (to->id == toNode) {
                return true;
            }
            stack.push_back(to->id);
        }
    }
    return false;
}

} // namespace gau
