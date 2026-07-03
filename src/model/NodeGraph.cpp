#include "NodeGraph.h"

NodeId NodeGraph::AddNode(const NodeClass& nodeClass, float x, float y)
{
    Node node;
    node.id = nextNodeId++;
    node.nodeClass = &nodeClass;
    node.x = x;
    node.y = y;
    for (const PinDef& pinDef : nodeClass.GetPins()) {
        AddPin(node, pinDef);
    }
    nodes.push_back(node);
    return nodes.back().id;
}

void NodeGraph::AddPin(Node& node, const PinDef& pinDef)
{
    Pin pin;
    pin.id = nextPinId++;
    pin.nodeId = node.id;
    pin.direction = pinDef.direction;
    pin.type = pinDef.type;
    pin.name = pinDef.name;

    if (pinDef.direction == PinDirection::Input) {
        node.inputs.push_back(pin);
    } else {
        node.outputs.push_back(pin);
    }
}

bool NodeGraph::RemoveNode(NodeId nodeId)
{
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].id == nodeId) {
            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

Node* NodeGraph::FindNode(NodeId nodeId)
{
    for (Node& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

const Node* NodeGraph::FindNode(NodeId nodeId) const
{
    for (const Node& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}
