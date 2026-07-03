#pragma once

#include "GraphClipboard.h"
#include "GraphTypes.h"
#include "NodeClass.h"
#include "NodeGraph.h"
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

// Connects two pins (validated via NodeGraph::CanConnect). When the
// input pin was already connected, the old link is replaced and
// restored on undo.
class AddLinkCommand : public GraphCommand
{
public:
    AddLinkCommand(PinId pinA, PinId pinB);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    PinId pinA;
    PinId pinB;
    LinkId createdLinkId = INVALID_ID;
    PinId replacedFromPin = INVALID_ID;
    PinId replacedToPin = INVALID_ID;
};

// Removes one or more links (alt-click on a link or a pin) as a single
// undoable step.
class RemoveLinksCommand : public GraphCommand
{
public:
    explicit RemoveLinksCommand(std::vector<LinkId> linkIds);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    struct Endpoints
    {
        PinId fromPinId = INVALID_ID;
        PinId toPinId = INVALID_ID;
    };

    std::vector<LinkId> linkIds;
    std::vector<Endpoints> removedEndpoints;
};

// Deletes nodes (Delete key / context menu) together with their links;
// undo restores everything verbatim including property values and
// reroute waypoints.
class DeleteNodesCommand : public GraphCommand
{
public:
    explicit DeleteNodesCommand(std::vector<NodeId> nodeIds);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    std::vector<NodeId> nodeIds;
    std::vector<Node> savedNodes;
    std::vector<Link> savedLinks;
};

// Instantiates the clipboard contents at a canvas position as one
// undoable step; the caller may select GetCreatedNodeIds afterwards.
class PasteClipboardCommand : public GraphCommand
{
public:
    PasteClipboardCommand(GraphClipboard clipboard, float x, float y);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

    const std::vector<NodeId>& GetCreatedNodeIds() const { return createdNodeIds; }

private:
    GraphClipboard clipboard;
    float x;
    float y;
    std::vector<NodeId> createdNodeIds;
};

// Deletes a comment box.
class DeleteCommentCommand : public GraphCommand
{
public:
    explicit DeleteCommentCommand(CommentId commentId);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    CommentId commentId;
    CommentNode savedComment;
};

// Sets one property value of a node instance (property panel edit).
class SetNodePropertyCommand : public GraphCommand
{
public:
    SetNodePropertyCommand(NodeId nodeId, int propertyIndex, PropertyValue newValue);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    NodeId nodeId;
    int propertyIndex;
    PropertyValue newValue;
    PropertyValue oldValue;
};

// Inserts a reroute waypoint into a link (ctrl-click on the curve).
class AddLinkPointCommand : public GraphCommand
{
public:
    AddLinkPointCommand(LinkId linkId, int insertIndex, float x, float y);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    LinkId linkId;
    int insertIndex;
    float x;
    float y;
};

// Records a completed drag of a reroute waypoint.
class MoveLinkPointCommand : public GraphCommand
{
public:
    MoveLinkPointCommand(LinkId linkId, int pointIndex,
                         float fromX, float fromY, float toX, float toY);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    LinkId linkId;
    int pointIndex;
    float fromX;
    float fromY;
    float toX;
    float toY;
};

// Removes a reroute waypoint (alt-click on it).
class RemoveLinkPointCommand : public GraphCommand
{
public:
    RemoveLinkPointCommand(LinkId linkId, int pointIndex);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    LinkId linkId;
    int pointIndex;
    float removedX = 0.0f;
    float removedY = 0.0f;
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
