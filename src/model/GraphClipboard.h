#pragma once

#include "NodeGraph.h"

#include <vector>

// Snapshot of one copied node: class, position relative to the copied
// selection's top-left corner, and its property values.
struct ClipboardNode
{
    const NodeClass* nodeClass = nullptr;
    float relX = 0.0f;
    float relY = 0.0f;
    std::vector<PropertyValue> propertyValues;
};

// A link between two copied nodes, referenced by clipboard-node index
// and pin position; waypoints are stored relative like node positions.
struct ClipboardLink
{
    int fromNodeIndex = -1;
    int fromPinIndex = -1;
    int toNodeIndex = -1;
    int toPinIndex = -1;
    std::vector<LinkPoint> relPoints;
};

// Application-wide clipboard (pastes across document tabs; NodeClass
// pointers stay valid because registered classes are never freed).
struct GraphClipboard
{
    std::vector<ClipboardNode> nodes;
    std::vector<ClipboardLink> links;

    bool IsEmpty() const { return nodes.empty(); }
};

// Snapshots the given nodes plus the links running between them.
GraphClipboard CopyNodesToClipboard(const NodeGraph& graph, const std::vector<NodeId>& nodeIds);
