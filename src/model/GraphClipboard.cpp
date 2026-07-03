#include "GraphClipboard.h"

GraphClipboard CopyNodesToClipboard(const NodeGraph& graph, const std::vector<NodeId>& nodeIds)
{
    GraphClipboard clipboard;

    std::vector<const Node*> sourceNodes;
    for (NodeId nodeId : nodeIds) {
        const Node* node = graph.FindNode(nodeId);
        if (node != nullptr) {
            sourceNodes.push_back(node);
        }
    }
    if (sourceNodes.empty()) {
        return clipboard;
    }

    float minX = sourceNodes.front()->x;
    float minY = sourceNodes.front()->y;
    for (const Node* node : sourceNodes) {
        if (node->x < minX) {
            minX = node->x;
        }
        if (node->y < minY) {
            minY = node->y;
        }
    }

    for (const Node* node : sourceNodes) {
        ClipboardNode clipNode;
        clipNode.nodeClass = node->nodeClass;
        clipNode.relX = node->x - minX;
        clipNode.relY = node->y - minY;
        clipNode.propertyValues = node->propertyValues;
        clipboard.nodes.push_back(std::move(clipNode));
    }

    // Pin -> (clipboard node index, pin index) lookup for both ends.
    auto findOutputPin = [&sourceNodes](PinId pinId, int& outNodeIndex, int& outPinIndex) {
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(sourceNodes.size()); ++nodeIndex) {
            const std::vector<Pin>& outputs = sourceNodes[static_cast<std::size_t>(nodeIndex)]->outputs;
            for (int pinIndex = 0; pinIndex < static_cast<int>(outputs.size()); ++pinIndex) {
                if (outputs[static_cast<std::size_t>(pinIndex)].id == pinId) {
                    outNodeIndex = nodeIndex;
                    outPinIndex = pinIndex;
                    return true;
                }
            }
        }
        return false;
    };
    auto findInputPin = [&sourceNodes](PinId pinId, int& outNodeIndex, int& outPinIndex) {
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(sourceNodes.size()); ++nodeIndex) {
            const std::vector<Pin>& inputs = sourceNodes[static_cast<std::size_t>(nodeIndex)]->inputs;
            for (int pinIndex = 0; pinIndex < static_cast<int>(inputs.size()); ++pinIndex) {
                if (inputs[static_cast<std::size_t>(pinIndex)].id == pinId) {
                    outNodeIndex = nodeIndex;
                    outPinIndex = pinIndex;
                    return true;
                }
            }
        }
        return false;
    };

    for (const Link& link : graph.GetLinks()) {
        ClipboardLink clipLink;
        if (!findOutputPin(link.fromPinId, clipLink.fromNodeIndex, clipLink.fromPinIndex)
            || !findInputPin(link.toPinId, clipLink.toNodeIndex, clipLink.toPinIndex)) {
            continue;
        }
        for (const LinkPoint& point : link.points) {
            LinkPoint relPoint;
            relPoint.x = point.x - minX;
            relPoint.y = point.y - minY;
            clipLink.relPoints.push_back(relPoint);
        }
        clipboard.links.push_back(std::move(clipLink));
    }

    return clipboard;
}
