#include "GraphCommands.h"
#include "NodeClass.h"
#include "NodeGraph.h"

#include <utility>

AddNodeCommand::AddNodeCommand(const NodeClass& nodeClass, float x, float y)
    : nodeClass(&nodeClass)
    , x(x)
    , y(y)
{
}

bool AddNodeCommand::Execute(NodeGraph& graph)
{
    createdNodeId = graph.AddNode(*nodeClass, x, y);
    return createdNodeId != INVALID_ID;
}

void AddNodeCommand::Undo(NodeGraph& graph)
{
    graph.RemoveNode(createdNodeId);
    createdNodeId = INVALID_ID;
}

MoveNodesCommand::MoveNodesCommand(std::vector<NodeMove> moves)
    : moves(std::move(moves))
{
}

bool MoveNodesCommand::Execute(NodeGraph& graph)
{
    bool anyApplied = false;
    for (const NodeMove& move : moves) {
        Node* node = graph.FindNode(move.nodeId);
        if (node != nullptr) {
            node->x = move.toX;
            node->y = move.toY;
            anyApplied = true;
        }
    }
    return anyApplied;
}

void MoveNodesCommand::Undo(NodeGraph& graph)
{
    for (const NodeMove& move : moves) {
        Node* node = graph.FindNode(move.nodeId);
        if (node != nullptr) {
            node->x = move.fromX;
            node->y = move.fromY;
        }
    }
}

AddLinkCommand::AddLinkCommand(PinId pinA, PinId pinB)
    : pinA(pinA)
    , pinB(pinB)
{
}

bool AddLinkCommand::Execute(NodeGraph& graph)
{
    if (!graph.CanConnect(pinA, pinB)) {
        return false;
    }
    PinId outputPin = INVALID_ID;
    PinId inputPin = INVALID_ID;
    if (!graph.NormalizeConnection(pinA, pinB, outputPin, inputPin)) {
        return false;
    }

    replacedFromPin = INVALID_ID;
    replacedToPin = INVALID_ID;
    // UE exclusivity: exec outputs hold one link, data inputs hold one.
    const Pin* outputPinData = graph.FindPin(outputPin);
    const Link* existing = (outputPinData != nullptr && outputPinData->type == PinType::Exec)
                               ? graph.FindLinkFromOutput(outputPin)
                               : graph.FindLinkToInput(inputPin);
    if (existing != nullptr) {
        replacedFromPin = existing->fromPinId;
        replacedToPin = existing->toPinId;
        graph.RemoveLink(existing->id);
    }

    createdLinkId = graph.AddLink(outputPin, inputPin);
    return createdLinkId != INVALID_ID;
}

void AddLinkCommand::Undo(NodeGraph& graph)
{
    graph.RemoveLink(createdLinkId);
    createdLinkId = INVALID_ID;
    if (replacedFromPin != INVALID_ID && replacedToPin != INVALID_ID) {
        graph.AddLink(replacedFromPin, replacedToPin);
    }
}

RemoveLinksCommand::RemoveLinksCommand(std::vector<LinkId> linkIds)
    : linkIds(std::move(linkIds))
{
}

bool RemoveLinksCommand::Execute(NodeGraph& graph)
{
    removedEndpoints.clear();
    for (LinkId linkId : linkIds) {
        const Link* link = graph.FindLink(linkId);
        if (link == nullptr) {
            continue;
        }
        Endpoints endpoints;
        endpoints.fromPinId = link->fromPinId;
        endpoints.toPinId = link->toPinId;
        removedEndpoints.push_back(endpoints);
        graph.RemoveLink(linkId);
    }
    return !removedEndpoints.empty();
}

void RemoveLinksCommand::Undo(NodeGraph& graph)
{
    // Re-created links get new ids; refresh the list for redo.
    linkIds.clear();
    for (const Endpoints& endpoints : removedEndpoints) {
        linkIds.push_back(graph.AddLink(endpoints.fromPinId, endpoints.toPinId));
    }
}

DeleteNodesCommand::DeleteNodesCommand(std::vector<NodeId> nodeIds)
    : nodeIds(std::move(nodeIds))
{
}

bool DeleteNodesCommand::Execute(NodeGraph& graph)
{
    savedNodes.clear();
    savedLinks.clear();

    for (NodeId nodeId : nodeIds) {
        const Node* node = graph.FindNode(nodeId);
        if (node != nullptr) {
            savedNodes.push_back(*node);
        }
    }
    if (savedNodes.empty()) {
        return false;
    }

    auto pinBelongsToSaved = [this](PinId pinId) {
        for (const Node& node : savedNodes) {
            for (const Pin& pin : node.inputs) {
                if (pin.id == pinId) {
                    return true;
                }
            }
            for (const Pin& pin : node.outputs) {
                if (pin.id == pinId) {
                    return true;
                }
            }
        }
        return false;
    };
    for (const Link& link : graph.GetLinks()) {
        if (pinBelongsToSaved(link.fromPinId) || pinBelongsToSaved(link.toPinId)) {
            savedLinks.push_back(link);
        }
    }

    for (const Node& node : savedNodes) {
        graph.RemoveNode(node.id);
    }
    return true;
}

void DeleteNodesCommand::Undo(NodeGraph& graph)
{
    for (const Node& node : savedNodes) {
        graph.RestoreNode(node);
    }
    for (const Link& link : savedLinks) {
        graph.RestoreLink(link);
    }
}

PasteClipboardCommand::PasteClipboardCommand(GraphClipboard clipboard, float x, float y)
    : clipboard(std::move(clipboard))
    , x(x)
    , y(y)
{
}

bool PasteClipboardCommand::Execute(NodeGraph& graph)
{
    createdNodeIds.clear();

    for (const ClipboardNode& clipNode : clipboard.nodes) {
        if (clipNode.nodeClass == nullptr) {
            createdNodeIds.push_back(INVALID_ID);
            continue;
        }
        const NodeId nodeId = graph.AddNode(*clipNode.nodeClass,
                                            x + clipNode.relX, y + clipNode.relY);
        Node* node = graph.FindNode(nodeId);
        if (node != nullptr) {
            // The class may have changed since the copy; keep in range.
            const std::size_t count =
                (clipNode.propertyValues.size() < node->propertyValues.size())
                    ? clipNode.propertyValues.size()
                    : node->propertyValues.size();
            for (std::size_t i = 0; i < count; ++i) {
                node->propertyValues[i] = clipNode.propertyValues[i];
            }
        }
        createdNodeIds.push_back(nodeId);
    }

    bool anyCreated = false;
    for (NodeId nodeId : createdNodeIds) {
        if (nodeId != INVALID_ID) {
            anyCreated = true;
            break;
        }
    }
    if (!anyCreated) {
        return false;
    }

    for (const ClipboardLink& clipLink : clipboard.links) {
        if (clipLink.fromNodeIndex < 0
            || clipLink.fromNodeIndex >= static_cast<int>(createdNodeIds.size())
            || clipLink.toNodeIndex < 0
            || clipLink.toNodeIndex >= static_cast<int>(createdNodeIds.size())) {
            continue;
        }
        const Node* fromNode =
            graph.FindNode(createdNodeIds[static_cast<std::size_t>(clipLink.fromNodeIndex)]);
        const Node* toNode =
            graph.FindNode(createdNodeIds[static_cast<std::size_t>(clipLink.toNodeIndex)]);
        if (fromNode == nullptr || toNode == nullptr
            || clipLink.fromPinIndex < 0
            || clipLink.fromPinIndex >= static_cast<int>(fromNode->outputs.size())
            || clipLink.toPinIndex < 0
            || clipLink.toPinIndex >= static_cast<int>(toNode->inputs.size())) {
            continue;
        }
        const PinId outputPin =
            fromNode->outputs[static_cast<std::size_t>(clipLink.fromPinIndex)].id;
        const PinId inputPin =
            toNode->inputs[static_cast<std::size_t>(clipLink.toPinIndex)].id;
        if (!graph.CanConnect(outputPin, inputPin)) {
            continue;
        }
        const LinkId linkId = graph.AddLink(outputPin, inputPin);
        Link* link = graph.FindLink(linkId);
        if (link != nullptr) {
            for (const LinkPoint& relPoint : clipLink.relPoints) {
                LinkPoint point;
                point.x = x + relPoint.x;
                point.y = y + relPoint.y;
                link->points.push_back(point);
            }
        }
    }
    return true;
}

void PasteClipboardCommand::Undo(NodeGraph& graph)
{
    for (NodeId nodeId : createdNodeIds) {
        if (nodeId != INVALID_ID) {
            graph.RemoveNode(nodeId);
        }
    }
    createdNodeIds.clear();
}

DeleteCommentCommand::DeleteCommentCommand(CommentId commentId)
    : commentId(commentId)
{
}

bool DeleteCommentCommand::Execute(NodeGraph& graph)
{
    const CommentNode* comment = graph.FindComment(commentId);
    if (comment == nullptr) {
        return false;
    }
    savedComment = *comment;
    return graph.RemoveComment(commentId);
}

void DeleteCommentCommand::Undo(NodeGraph& graph)
{
    graph.RestoreComment(savedComment);
}

SetNodePropertyCommand::SetNodePropertyCommand(NodeId nodeId, int propertyIndex,
                                               PropertyValue newValue)
    : nodeId(nodeId)
    , propertyIndex(propertyIndex)
    , newValue(std::move(newValue))
{
}

bool SetNodePropertyCommand::Execute(NodeGraph& graph)
{
    Node* node = graph.FindNode(nodeId);
    if (node == nullptr || propertyIndex < 0
        || propertyIndex >= static_cast<int>(node->propertyValues.size())) {
        return false;
    }
    oldValue = node->propertyValues[static_cast<std::size_t>(propertyIndex)];
    node->propertyValues[static_cast<std::size_t>(propertyIndex)] = newValue;
    return true;
}

void SetNodePropertyCommand::Undo(NodeGraph& graph)
{
    Node* node = graph.FindNode(nodeId);
    if (node != nullptr && propertyIndex >= 0
        && propertyIndex < static_cast<int>(node->propertyValues.size())) {
        node->propertyValues[static_cast<std::size_t>(propertyIndex)] = oldValue;
    }
}

AddLinkPointCommand::AddLinkPointCommand(LinkId linkId, int insertIndex, float x, float y)
    : linkId(linkId)
    , insertIndex(insertIndex)
    , x(x)
    , y(y)
{
}

bool AddLinkPointCommand::Execute(NodeGraph& graph)
{
    Link* link = graph.FindLink(linkId);
    if (link == nullptr || insertIndex < 0
        || insertIndex > static_cast<int>(link->points.size())) {
        return false;
    }
    LinkPoint point;
    point.x = x;
    point.y = y;
    link->points.insert(link->points.begin() + insertIndex, point);
    return true;
}

void AddLinkPointCommand::Undo(NodeGraph& graph)
{
    Link* link = graph.FindLink(linkId);
    if (link != nullptr && insertIndex >= 0
        && insertIndex < static_cast<int>(link->points.size())) {
        link->points.erase(link->points.begin() + insertIndex);
    }
}

MoveLinkPointCommand::MoveLinkPointCommand(LinkId linkId, int pointIndex,
                                           float fromX, float fromY, float toX, float toY)
    : linkId(linkId)
    , pointIndex(pointIndex)
    , fromX(fromX)
    , fromY(fromY)
    , toX(toX)
    , toY(toY)
{
}

bool MoveLinkPointCommand::Execute(NodeGraph& graph)
{
    Link* link = graph.FindLink(linkId);
    if (link == nullptr || pointIndex < 0
        || pointIndex >= static_cast<int>(link->points.size())) {
        return false;
    }
    link->points[static_cast<std::size_t>(pointIndex)].x = toX;
    link->points[static_cast<std::size_t>(pointIndex)].y = toY;
    return true;
}

void MoveLinkPointCommand::Undo(NodeGraph& graph)
{
    Link* link = graph.FindLink(linkId);
    if (link != nullptr && pointIndex >= 0
        && pointIndex < static_cast<int>(link->points.size())) {
        link->points[static_cast<std::size_t>(pointIndex)].x = fromX;
        link->points[static_cast<std::size_t>(pointIndex)].y = fromY;
    }
}

RemoveLinkPointCommand::RemoveLinkPointCommand(LinkId linkId, int pointIndex)
    : linkId(linkId)
    , pointIndex(pointIndex)
{
}

bool RemoveLinkPointCommand::Execute(NodeGraph& graph)
{
    Link* link = graph.FindLink(linkId);
    if (link == nullptr || pointIndex < 0
        || pointIndex >= static_cast<int>(link->points.size())) {
        return false;
    }
    removedX = link->points[static_cast<std::size_t>(pointIndex)].x;
    removedY = link->points[static_cast<std::size_t>(pointIndex)].y;
    link->points.erase(link->points.begin() + pointIndex);
    return true;
}

void RemoveLinkPointCommand::Undo(NodeGraph& graph)
{
    Link* link = graph.FindLink(linkId);
    if (link != nullptr && pointIndex >= 0
        && pointIndex <= static_cast<int>(link->points.size())) {
        LinkPoint point;
        point.x = removedX;
        point.y = removedY;
        link->points.insert(link->points.begin() + pointIndex, point);
    }
}

AddCommentCommand::AddCommentCommand(std::string title, float x, float y,
                                     float width, float height)
    : title(std::move(title))
    , x(x)
    , y(y)
    , width(width)
    , height(height)
{
}

bool AddCommentCommand::Execute(NodeGraph& graph)
{
    createdCommentId = graph.AddComment(title, x, y, width, height);
    return createdCommentId != INVALID_ID;
}

void AddCommentCommand::Undo(NodeGraph& graph)
{
    graph.RemoveComment(createdCommentId);
    createdCommentId = INVALID_ID;
}

MoveCommentCommand::MoveCommentCommand(CommentId commentId, float fromX, float fromY,
                                       float toX, float toY,
                                       std::vector<NodeMove> containedMoves)
    : commentId(commentId)
    , fromX(fromX)
    , fromY(fromY)
    , toX(toX)
    , toY(toY)
    , containedMoves(std::move(containedMoves))
{
}

bool MoveCommentCommand::Execute(NodeGraph& graph)
{
    CommentNode* comment = graph.FindComment(commentId);
    if (comment == nullptr) {
        return false;
    }
    comment->x = toX;
    comment->y = toY;
    for (const NodeMove& move : containedMoves) {
        Node* node = graph.FindNode(move.nodeId);
        if (node != nullptr) {
            node->x = move.toX;
            node->y = move.toY;
        }
    }
    return true;
}

void MoveCommentCommand::Undo(NodeGraph& graph)
{
    CommentNode* comment = graph.FindComment(commentId);
    if (comment != nullptr) {
        comment->x = fromX;
        comment->y = fromY;
    }
    for (const NodeMove& move : containedMoves) {
        Node* node = graph.FindNode(move.nodeId);
        if (node != nullptr) {
            node->x = move.fromX;
            node->y = move.fromY;
        }
    }
}

ResizeCommentCommand::ResizeCommentCommand(CommentId commentId, float fromWidth, float fromHeight,
                                           float toWidth, float toHeight)
    : commentId(commentId)
    , fromWidth(fromWidth)
    , fromHeight(fromHeight)
    , toWidth(toWidth)
    , toHeight(toHeight)
{
}

bool ResizeCommentCommand::Execute(NodeGraph& graph)
{
    CommentNode* comment = graph.FindComment(commentId);
    if (comment == nullptr) {
        return false;
    }
    comment->width = toWidth;
    comment->height = toHeight;
    return true;
}

void ResizeCommentCommand::Undo(NodeGraph& graph)
{
    CommentNode* comment = graph.FindComment(commentId);
    if (comment != nullptr) {
        comment->width = fromWidth;
        comment->height = fromHeight;
    }
}

SetCommentTitleCommand::SetCommentTitleCommand(CommentId commentId, std::string oldTitle,
                                               std::string newTitle)
    : commentId(commentId)
    , oldTitle(std::move(oldTitle))
    , newTitle(std::move(newTitle))
{
}

bool SetCommentTitleCommand::Execute(NodeGraph& graph)
{
    CommentNode* comment = graph.FindComment(commentId);
    if (comment == nullptr) {
        return false;
    }
    comment->title = newTitle;
    return true;
}

void SetCommentTitleCommand::Undo(NodeGraph& graph)
{
    CommentNode* comment = graph.FindComment(commentId);
    if (comment != nullptr) {
        comment->title = oldTitle;
    }
}
