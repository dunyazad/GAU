#pragma once

#include "GraphTypes.h"
#include "NodeClass.h"

#include <string>
#include <vector>

struct Pin
{
    PinId id = INVALID_ID;
    NodeId nodeId = INVALID_ID;
    PinDirection direction = PinDirection::Input;
    PinType type = PinType::Exec;
    std::string name;
};

struct Node
{
    NodeId id = INVALID_ID;
    // Meta-object describing this node's type. Never null for nodes
    // created through NodeGraph::AddNode.
    const NodeClass* nodeClass = nullptr;
    // Canvas-space position of the node's top-left corner.
    float x = 0.0f;
    float y = 0.0f;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
};

// Container for nodes/pins (links arrive in M4). Mutations that belong to
// user edits must go through UndoStack commands, never called directly
// from interaction code.
class NodeGraph
{
public:
    // Instantiates a node of the given class at a canvas position,
    // creating its pins from the class pin definitions.
    NodeId AddNode(const NodeClass& nodeClass, float x, float y);
    bool RemoveNode(NodeId nodeId);

    Node* FindNode(NodeId nodeId);
    const Node* FindNode(NodeId nodeId) const;
    const std::vector<Node>& GetNodes() const { return nodes; }

private:
    void AddPin(Node& node, const PinDef& pinDef);

    std::vector<Node> nodes;
    std::uint32_t nextNodeId = 1;
    std::uint32_t nextPinId = 1;
};
