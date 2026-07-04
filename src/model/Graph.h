#pragma once

// v2 graph container: owns nodes and links, creates node instances from a
// NodeClass, and validates connections against the core type system.

#include "Ids.h"
#include "Link.h"
#include "Node.h"
#include "NodeClassV2.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gau {

class TypeRegistry;

class Graph
{
public:
    explicit Graph(const TypeRegistry& types) : types(&types) {}

    // Instantiates a node of the class, creating pins and default property
    // values. Returns the new node id.
    NodeId AddNode(const NodeClass& nodeClass, float x, float y);
    bool RemoveNode(NodeId nodeId);

    Node* FindNode(NodeId nodeId);
    const Node* FindNode(NodeId nodeId) const;
    const std::vector<Node>& Nodes() const { return nodes; }

    // Appends a pin to an existing node with a fresh pin id, leaving other
    // pins (and their links) untouched. Used when a function gains a
    // parameter and existing instances must grow to match. Returns the new
    // pin id, or INVALID_ID if the node does not exist.
    PinId AppendPin(NodeId nodeId, PinDirection direction, TypeId type, const std::string& name);

    const Pin* FindPin(PinId pinId) const;
    const Node* FindPinOwner(PinId pinId) const;

    // Connection rules: pins exist, different nodes, opposite directions,
    // identical types, and (for exec pins) no exec cycle.
    bool CanConnect(PinId a, PinId b) const;
    // Orders a pin pair into (output, input). False if not opposite.
    bool Normalize(PinId a, PinId b, PinId& outOutput, PinId& outInput) const;

    // Creates a link (validate with CanConnect first). Returns INVALID_ID
    // on failure.
    LinkId AddLink(PinId outputPin, PinId inputPin);
    bool RemoveLink(LinkId linkId);
    const std::vector<Link>& Links() const { return links; }

private:
    bool IsExecPin(PinId pinId) const;
    void RemoveLinksTouchingNode(NodeId nodeId);
    bool ExecPathExists(NodeId fromNode, NodeId toNode) const;

    const TypeRegistry* types;
    std::vector<Node> nodes;
    std::vector<Link> links;
    std::uint32_t nextNodeId = 1;
    std::uint32_t nextPinId = 1;
    std::uint32_t nextLinkId = 1;
};

} // namespace gau
