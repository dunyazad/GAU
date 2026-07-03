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

// Comment (group) box: a titled translucent rectangle drawn behind
// nodes. Dragging its title bar moves nodes fully contained in it.
struct CommentNode
{
    CommentId id = INVALID_ID;
    std::string title;
    // Canvas-space rect; the title bar occupies the top
    // COMMENT_TITLE_HEIGHT of it.
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
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
    // Per-instance property values, parallel to nodeClass->GetProperties();
    // initialized from the class defaults at spawn time.
    std::vector<PropertyValue> propertyValues;
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

    // Re-creates pins and property values of all nodes of a class after
    // its definition changed (class editor). Pin ids are re-assigned;
    // links to those pins must be dropped by the caller (none before M4).
    void RebuildNodesOfClass(const NodeClass& nodeClass);

    Node* FindNode(NodeId nodeId);
    const Node* FindNode(NodeId nodeId) const;
    const std::vector<Node>& GetNodes() const { return nodes; }

    CommentId AddComment(const std::string& title, float x, float y, float width, float height);
    bool RemoveComment(CommentId commentId);
    CommentNode* FindComment(CommentId commentId);
    const CommentNode* FindComment(CommentId commentId) const;
    const std::vector<CommentNode>& GetComments() const { return comments; }

private:
    void AddPin(Node& node, const PinDef& pinDef);

    std::vector<Node> nodes;
    std::vector<CommentNode> comments;
    std::uint32_t nextNodeId = 1;
    std::uint32_t nextPinId = 1;
    std::uint32_t nextCommentId = 1;
};
