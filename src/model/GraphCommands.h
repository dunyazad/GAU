#pragma once

#include "GraphTypes.h"
#include "UndoStack.h"

#include <string>
#include <vector>

class NodeClass;

// Creates a node of a given class at a canvas position.
class AddNodeCommand : public GraphCommand
{
public:
    AddNodeCommand(const NodeClass& nodeClass, float x, float y);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    const NodeClass* nodeClass;
    float x;
    float y;
    NodeId createdNodeId = INVALID_ID;
};

struct NodeMove
{
    NodeId nodeId = INVALID_ID;
    float fromX = 0.0f;
    float fromY = 0.0f;
    float toX = 0.0f;
    float toY = 0.0f;
};

// Records a completed drag of one or more nodes so it can be undone as
// a single step. Nodes are moved live during the drag; this command is
// pushed on release.
class MoveNodesCommand : public GraphCommand
{
public:
    explicit MoveNodesCommand(std::vector<NodeMove> moves);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    std::vector<NodeMove> moves;
};

// Creates a comment (group) box.
class AddCommentCommand : public GraphCommand
{
public:
    AddCommentCommand(std::string title, float x, float y, float width, float height);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    std::string title;
    float x;
    float y;
    float width;
    float height;
    CommentId createdCommentId = INVALID_ID;
};

// Records a completed drag of a comment box together with the nodes it
// contained at drag start.
class MoveCommentCommand : public GraphCommand
{
public:
    MoveCommentCommand(CommentId commentId, float fromX, float fromY, float toX, float toY,
                       std::vector<NodeMove> containedMoves);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    CommentId commentId;
    float fromX;
    float fromY;
    float toX;
    float toY;
    std::vector<NodeMove> containedMoves;
};

// Records a completed resize of a comment box (top-left corner fixed).
class ResizeCommentCommand : public GraphCommand
{
public:
    ResizeCommentCommand(CommentId commentId, float fromWidth, float fromHeight,
                         float toWidth, float toHeight);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    CommentId commentId;
    float fromWidth;
    float fromHeight;
    float toWidth;
    float toHeight;
};

// Applies an inline title edit of a comment box.
class SetCommentTitleCommand : public GraphCommand
{
public:
    SetCommentTitleCommand(CommentId commentId, std::string oldTitle, std::string newTitle);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    CommentId commentId;
    std::string oldTitle;
    std::string newTitle;
};
