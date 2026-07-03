#include "HitTest.h"

#include "model/NodeGraph.h"
#include "render/NodeLayoutCache.h"

std::vector<NodeId> HitTestNodesInRect(const NodeLayoutCache& layoutCache,
                                       float x0, float y0, float x1, float y1)
{
    const float left = (x0 < x1) ? x0 : x1;
    const float right = (x0 < x1) ? x1 : x0;
    const float top = (y0 < y1) ? y0 : y1;
    const float bottom = (y0 < y1) ? y1 : y0;

    std::vector<NodeId> result;
    for (const NodeLayout& layout : layoutCache.GetAll()) {
        const bool intersects = layout.x <= right && layout.x + layout.width >= left
                             && layout.y <= bottom && layout.y + layout.height >= top;
        if (intersects) {
            result.push_back(layout.nodeId);
        }
    }
    return result;
}

NodeId HitTestNode(const NodeLayoutCache& layoutCache, float canvasX, float canvasY)
{
    const std::vector<NodeLayout>& layouts = layoutCache.GetAll();
    // Iterate backwards: later layouts draw on top.
    for (std::size_t i = layouts.size(); i > 0; --i) {
        const NodeLayout& layout = layouts[i - 1];
        if (canvasX >= layout.x && canvasX <= layout.x + layout.width
            && canvasY >= layout.y && canvasY <= layout.y + layout.height) {
            return layout.nodeId;
        }
    }
    return INVALID_ID;
}

CommentId HitTestCommentTitle(const NodeGraph& graph, float canvasX, float canvasY)
{
    const std::vector<CommentNode>& comments = graph.GetComments();
    for (std::size_t i = comments.size(); i > 0; --i) {
        const CommentNode& comment = comments[i - 1];
        if (canvasX >= comment.x && canvasX <= comment.x + comment.width
            && canvasY >= comment.y && canvasY <= comment.y + COMMENT_TITLE_HEIGHT) {
            return comment.id;
        }
    }
    return INVALID_ID;
}

CommentId HitTestCommentResizeHandle(const NodeGraph& graph, float canvasX, float canvasY)
{
    const std::vector<CommentNode>& comments = graph.GetComments();
    for (std::size_t i = comments.size(); i > 0; --i) {
        const CommentNode& comment = comments[i - 1];
        const float right = comment.x + comment.width;
        const float bottom = comment.y + comment.height;
        if (canvasX >= right - COMMENT_RESIZE_HANDLE && canvasX <= right
            && canvasY >= bottom - COMMENT_RESIZE_HANDLE && canvasY <= bottom) {
            return comment.id;
        }
    }
    return INVALID_ID;
}

std::vector<NodeId> NodesContainedInComment(const NodeLayoutCache& layoutCache,
                                            const CommentNode& comment)
{
    std::vector<NodeId> result;
    for (const NodeLayout& layout : layoutCache.GetAll()) {
        const bool contained = layout.x >= comment.x
                            && layout.y >= comment.y
                            && layout.x + layout.width <= comment.x + comment.width
                            && layout.y + layout.height <= comment.y + comment.height;
        if (contained) {
            result.push_back(layout.nodeId);
        }
    }
    return result;
}
