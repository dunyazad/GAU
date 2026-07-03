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
