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
    // Persistent identity (UUID v4): survives save/load and undo, is
    // referenced by serialized links, and stays stable across sessions.
    // Pasted copies get a fresh guid. The integer id above remains the
    // fast runtime handle.
    std::string guid;
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

// Reroute waypoint on a link, canvas coordinates.
struct LinkPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

// A connection between an output pin and an input pin (normalized:
// fromPinId is always the output side). The curve routes through the
// optional waypoints in order.
struct Link
{
    LinkId id = INVALID_ID;
    PinId fromPinId = INVALID_ID;
    PinId toPinId = INVALID_ID;
    std::vector<LinkPoint> points;
};

// Container for nodes/pins/links. Mutations that belong to user edits
// must go through UndoStack commands, never called directly from
// interaction code.
class NodeGraph
{
public:
    // Instantiates a node of the given class at a canvas position,
    // creating its pins from the class pin definitions.
    NodeId AddNode(const NodeClass& nodeClass, float x, float y);
    // Also removes links attached to the node's pins.
    bool RemoveNode(NodeId nodeId);

    // Re-creates pins and property values of all nodes of a class after
    // its definition changed (class editor). Pin ids are re-assigned and
    // links attached to the old pins are removed.
    void RebuildNodesOfClass(const NodeClass& nodeClass);

    Node* FindNode(NodeId nodeId);
    const Node* FindNode(NodeId nodeId) const;
    const Node* FindNodeByGuid(const std::string& guid) const;
    const std::vector<Node>& GetNodes() const { return nodes; }

    const Pin* FindPin(PinId pinId) const;
    const Node* FindPinOwner(PinId pinId) const;

    // Single source of truth for connection rules: no same-node links,
    // directions must be opposite, types must match (which also forbids
    // exec-data mixing) and exec links must not create a cycle. An
    // occupied input pin is allowed (creation replaces the old link).
    bool CanConnect(PinId pinA, PinId pinB) const;

    // Orders an arbitrary pin pair into (output, input). Returns false
    // when the pins do not form an opposite-direction pair.
    bool NormalizeConnection(PinId pinA, PinId pinB,
                             PinId& outOutputPin, PinId& outInputPin) const;

    // Creates a link (callers validate with CanConnect first).
    LinkId AddLink(PinId outputPinId, PinId inputPinId);
    bool RemoveLink(LinkId linkId);
    Link* FindLink(LinkId linkId);
    const Link* FindLink(LinkId linkId) const;
    // Exclusive sides (UE rules): a data input and an exec output hold
    // at most one link; data outputs and exec inputs fan out freely.
    const Link* FindLinkToInput(PinId inputPinId) const;
    const Link* FindLinkFromOutput(PinId outputPinId) const;
    bool IsPinConnected(PinId pinId) const;
    const std::vector<Link>& GetLinks() const { return links; }

    // Re-insert previously removed elements verbatim (undo of delete
    // commands). Ids stay valid because id counters never reuse values.
    void RestoreNode(const Node& node) { nodes.push_back(node); }
    void RestoreLink(const Link& link) { links.push_back(link); }
    void RestoreComment(const CommentNode& comment) { comments.push_back(comment); }

    CommentId AddComment(const std::string& title, float x, float y, float width, float height);
    bool RemoveComment(CommentId commentId);
    CommentNode* FindComment(CommentId commentId);
    const CommentNode* FindComment(CommentId commentId) const;
    const std::vector<CommentNode>& GetComments() const { return comments; }

private:
    void AddPin(Node& node, const PinDef& pinDef);
    void RemoveLinksTouchingNode(const Node& node);
    // True when execution starting at fromNode can reach toNode by
    // following exec links.
    bool ExecPathExists(NodeId fromNodeId, NodeId toNodeId) const;

    std::vector<Node> nodes;
    std::vector<CommentNode> comments;
    std::vector<Link> links;
    std::uint32_t nextNodeId = 1;
    std::uint32_t nextPinId = 1;
    std::uint32_t nextCommentId = 1;
    std::uint32_t nextLinkId = 1;
};
