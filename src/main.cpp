#include "platform/PlatformWindow.h"
#include "platform/PlatformNVG.h"
#include "platform/PlatformFileDialog.h"
#include "render/Canvas.h"
#include "render/GridRenderer.h"
#include "render/SelectionRenderer.h"
#include "render/CommentRenderer.h"
#include "render/LinkRenderer.h"
#include "render/TabBarRenderer.h"
#include "render/NodeRenderer.h"
#include "render/NodeLayoutCache.h"
#include "render/ContextMenuRenderer.h"
#include "model/NodeGraph.h"
#include "model/NodeClassLoader.h"
#include "model/EditorSettings.h"
#include "model/GraphSerializer.h"
#include "model/UndoStack.h"
#include "model/GraphCommands.h"
#include "interaction/EditorInputEvent.h"
#include "interaction/ContextMenu.h"
#include "interaction/ClassEditorDialog.h"
#include "interaction/ActionMenu.h"
#include "interaction/HitTest.h"
#include "interaction/PropertyPanel.h"
#include "interaction/TabBar.h"
#include "render/ActionMenuRenderer.h"
#include "model/PropertyText.h"
#include "render/PropertyPanelRenderer.h"
#include "render/ClassEditorDialogRenderer.h"

#include <nanovg.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Canvas background color from the design spec: rgb(30,30,33).
static const float BACKGROUND_R = 30.0f / 255.0f;
static const float BACKGROUND_G = 30.0f / 255.0f;
static const float BACKGROUND_B = 33.0f / 255.0f;

// Zoom multiplier per wheel notch.
static const float ZOOM_STEP = 1.1f;

// Initial canvas zoom: node text renders at 2x the spec sizes for
// readability while canvas-unit metrics stay per the design spec.
static const float DEFAULT_CANVAS_ZOOM = 2.0f;

// Right-button release with less accumulated motion than this is a click
// (opens the context menu); more is a pan drag.
static const float CLICK_DRAG_THRESHOLD = 4.0f;

// Temporary M1/M2 input wiring: right-drag pan, wheel zoom, right-click
// detection for the context menu, left-drag node move (whole selection)
// and rubber-band selection on empty canvas. Replaced by InteractionFSM
// in M3.
struct CanvasController
{
    bool panning = false;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    float dragDistance = 0.0f;

    std::vector<NodeId> selectedNodes;
    bool draggingNodes = false;
    bool nodeMoved = false;
    std::vector<NodeMove> dragMoves;

    bool rubberBanding = false;
    float bandStartCanvasX = 0.0f;
    float bandStartCanvasY = 0.0f;
    float bandEndCanvasX = 0.0f;
    float bandEndCanvasY = 0.0f;

    // Link dragging from a pin.
    PinId draggingLinkPinId = INVALID_ID;
    float linkDragCanvasX = 0.0f;
    float linkDragCanvasY = 0.0f;

    // Reroute waypoint dragging.
    LinkId draggingPointLinkId = INVALID_ID;
    int draggingPointIndex = -1;
    float pointFromX = 0.0f;
    float pointFromY = 0.0f;
    bool pointMoved = false;

    // Comment box interactions.
    CommentId draggingCommentId = INVALID_ID;
    float commentFromX = 0.0f;
    float commentFromY = 0.0f;
    float commentDragDistance = 0.0f;
    std::vector<NodeMove> commentContainedMoves;
    CommentId resizingCommentId = INVALID_ID;
    float resizeFromWidth = 0.0f;
    float resizeFromHeight = 0.0f;
    CommentId editingCommentId = INVALID_ID;
    std::string titleEditText;
    std::string titleEditOriginal;

    bool IsEditingTitle() const { return editingCommentId != INVALID_ID; }

    bool IsSelected(NodeId nodeId) const
    {
        for (NodeId selected : selectedNodes) {
            if (selected == nodeId) {
                return true;
            }
        }
        return false;
    }

    void BeginNodeDrag(NodeGraph& graph)
    {
        draggingNodes = true;
        nodeMoved = false;
        dragMoves.clear();
        for (NodeId nodeId : selectedNodes) {
            const Node* node = graph.FindNode(nodeId);
            if (node != nullptr) {
                NodeMove move;
                move.nodeId = nodeId;
                move.fromX = node->x;
                move.fromY = node->y;
                dragMoves.push_back(move);
            }
        }
    }

    void CommitTitleEdit(NodeGraph& graph, UndoStack& undoStack)
    {
        if (editingCommentId == INVALID_ID) {
            return;
        }
        if (titleEditText != titleEditOriginal) {
            undoStack.Execute(
                std::make_unique<SetCommentTitleCommand>(editingCommentId, titleEditOriginal,
                                                         titleEditText),
                graph);
        }
        editingCommentId = INVALID_ID;
        titleEditText.clear();
        titleEditOriginal.clear();
    }

    void CancelTitleEdit()
    {
        editingCommentId = INVALID_ID;
        titleEditText.clear();
        titleEditOriginal.clear();
    }

    void BeginCommentDrag(const CommentNode& comment, NodeGraph& graph,
                          const NodeLayoutCache& layoutCache)
    {
        draggingCommentId = comment.id;
        commentFromX = comment.x;
        commentFromY = comment.y;
        commentDragDistance = 0.0f;
        commentContainedMoves.clear();
        for (NodeId nodeId : NodesContainedInComment(layoutCache, comment)) {
            const Node* node = graph.FindNode(nodeId);
            if (node != nullptr) {
                NodeMove move;
                move.nodeId = nodeId;
                move.fromX = node->x;
                move.fromY = node->y;
                commentContainedMoves.push_back(move);
            }
        }
    }

    void EndCommentDrag(NodeGraph& graph, UndoStack& undoStack)
    {
        const CommentNode* comment = graph.FindComment(draggingCommentId);
        if (comment != nullptr) {
            if (commentDragDistance < CLICK_DRAG_THRESHOLD) {
                // A click on the title bar starts inline renaming.
                editingCommentId = comment->id;
                titleEditText = comment->title;
                titleEditOriginal = comment->title;
            } else {
                for (NodeMove& move : commentContainedMoves) {
                    const Node* node = graph.FindNode(move.nodeId);
                    if (node != nullptr) {
                        move.toX = node->x;
                        move.toY = node->y;
                    } else {
                        move.toX = move.fromX;
                        move.toY = move.fromY;
                    }
                }
                undoStack.Execute(
                    std::make_unique<MoveCommentCommand>(comment->id, commentFromX, commentFromY,
                                                         comment->x, comment->y,
                                                         commentContainedMoves),
                    graph);
            }
        }
        draggingCommentId = INVALID_ID;
        commentContainedMoves.clear();
    }

    void EndCommentResize(NodeGraph& graph, UndoStack& undoStack)
    {
        const CommentNode* comment = graph.FindComment(resizingCommentId);
        if (comment != nullptr
            && (comment->width != resizeFromWidth || comment->height != resizeFromHeight)) {
            undoStack.Execute(
                std::make_unique<ResizeCommentCommand>(comment->id, resizeFromWidth,
                                                       resizeFromHeight, comment->width,
                                                       comment->height),
                graph);
        }
        resizingCommentId = INVALID_ID;
    }

    void EndNodeDrag(NodeGraph& graph, UndoStack& undoStack)
    {
        draggingNodes = false;
        if (!nodeMoved) {
            return;
        }
        for (NodeMove& move : dragMoves) {
            const Node* node = graph.FindNode(move.nodeId);
            if (node != nullptr) {
                move.toX = node->x;
                move.toY = node->y;
            } else {
                move.toX = move.fromX;
                move.toY = move.fromY;
            }
        }
        undoStack.Execute(std::make_unique<MoveNodesCommand>(dragMoves), graph);
        dragMoves.clear();
    }

    // Returns true when a right click (press+release without dragging)
    // completed; the caller opens the context menu at the event position.
    bool HandleEvent(const EditorInputEvent& event, Canvas& canvas, NodeGraph& graph,
                     const NodeLayoutCache& layoutCache, UndoStack& undoStack)
    {
        switch (event.type) {
        case EditorInputType::MouseDown:
            if (IsEditingTitle()) {
                // Any click commits the rename, then handling continues.
                CommitTitleEdit(graph, undoStack);
            }
            if (event.button == EditorMouseButton::Right) {
                panning = true;
                dragDistance = 0.0f;
                lastMouseX = event.x;
                lastMouseY = event.y;
            } else if (event.button == EditorMouseButton::Left) {
                const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{event.x, event.y});
                lastMouseX = event.x;
                lastMouseY = event.y;
                // Alt-click breaks links: on a pin all of its links, on
                // a waypoint that waypoint, on a link that one link.
                if (event.alt) {
                    const PinId altPinId = HitTestPin(layoutCache, canvasPos.x, canvasPos.y);
                    if (altPinId != INVALID_ID) {
                        std::vector<LinkId> pinLinks;
                        for (const Link& link : graph.GetLinks()) {
                            if (link.fromPinId == altPinId || link.toPinId == altPinId) {
                                pinLinks.push_back(link.id);
                            }
                        }
                        if (!pinLinks.empty()) {
                            undoStack.Execute(
                                std::make_unique<RemoveLinksCommand>(std::move(pinLinks)),
                                graph);
                        }
                        break;
                    }
                    const LinkPointHit pointHit =
                        HitTestLinkPoint(graph, canvasPos.x, canvasPos.y);
                    if (pointHit.linkId != INVALID_ID) {
                        undoStack.Execute(
                            std::make_unique<RemoveLinkPointCommand>(pointHit.linkId,
                                                                     pointHit.pointIndex),
                            graph);
                        break;
                    }
                    const LinkId altLinkId =
                        HitTestLink(graph, layoutCache, canvasPos.x, canvasPos.y);
                    if (altLinkId != INVALID_ID) {
                        undoStack.Execute(
                            std::make_unique<RemoveLinksCommand>(
                                std::vector<LinkId>{altLinkId}),
                            graph);
                        break;
                    }
                    break;
                }

                // Hit priority: pin > waypoint > node > link > comment.
                const PinId hitPinId = HitTestPin(layoutCache, canvasPos.x, canvasPos.y);
                if (hitPinId != INVALID_ID) {
                    draggingLinkPinId = hitPinId;
                    linkDragCanvasX = canvasPos.x;
                    linkDragCanvasY = canvasPos.y;
                    break;
                }
                const LinkPointHit pointHit = HitTestLinkPoint(graph, canvasPos.x, canvasPos.y);
                if (pointHit.linkId != INVALID_ID) {
                    const Link* link = graph.FindLink(pointHit.linkId);
                    if (link != nullptr) {
                        draggingPointLinkId = pointHit.linkId;
                        draggingPointIndex = pointHit.pointIndex;
                        pointFromX = link->points[static_cast<std::size_t>(pointHit.pointIndex)].x;
                        pointFromY = link->points[static_cast<std::size_t>(pointHit.pointIndex)].y;
                        pointMoved = false;
                    }
                    break;
                }
                const NodeId hitNodeId = HitTestNode(layoutCache, canvasPos.x, canvasPos.y);
                if (hitNodeId == INVALID_ID && event.ctrl) {
                    // Ctrl-click on a link inserts a reroute waypoint
                    // and starts dragging it right away.
                    int segmentIndex = 0;
                    const LinkId ctrlLinkId = HitTestLink(graph, layoutCache,
                                                          canvasPos.x, canvasPos.y,
                                                          &segmentIndex);
                    if (ctrlLinkId != INVALID_ID) {
                        if (undoStack.Execute(
                                std::make_unique<AddLinkPointCommand>(ctrlLinkId, segmentIndex,
                                                                      canvasPos.x, canvasPos.y),
                                graph)) {
                            draggingPointLinkId = ctrlLinkId;
                            draggingPointIndex = segmentIndex;
                            pointFromX = canvasPos.x;
                            pointFromY = canvasPos.y;
                            pointMoved = false;
                        }
                        break;
                    }
                }
                if (hitNodeId != INVALID_ID) {
                    // Clicking an unselected node makes it the selection;
                    // dragging moves the whole selection.
                    if (!IsSelected(hitNodeId)) {
                        selectedNodes.clear();
                        selectedNodes.push_back(hitNodeId);
                    }
                    BeginNodeDrag(graph);
                    break;
                }
                const CommentId resizeId =
                    HitTestCommentResizeHandle(graph, canvasPos.x, canvasPos.y);
                if (resizeId != INVALID_ID) {
                    const CommentNode* comment = graph.FindComment(resizeId);
                    if (comment != nullptr) {
                        resizingCommentId = resizeId;
                        resizeFromWidth = comment->width;
                        resizeFromHeight = comment->height;
                    }
                    break;
                }
                const CommentId titleId = HitTestCommentTitle(graph, canvasPos.x, canvasPos.y);
                if (titleId != INVALID_ID) {
                    const CommentNode* comment = graph.FindComment(titleId);
                    if (comment != nullptr) {
                        BeginCommentDrag(*comment, graph, layoutCache);
                    }
                    break;
                }
                selectedNodes.clear();
                rubberBanding = true;
                bandStartCanvasX = canvasPos.x;
                bandStartCanvasY = canvasPos.y;
                bandEndCanvasX = canvasPos.x;
                bandEndCanvasY = canvasPos.y;
            }
            break;

        case EditorInputType::MouseUp:
            if (event.button == EditorMouseButton::Right && panning) {
                panning = false;
                if (dragDistance < CLICK_DRAG_THRESHOLD) {
                    return true;
                }
            } else if (event.button == EditorMouseButton::Left) {
                if (draggingLinkPinId != INVALID_ID) {
                    const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{event.x, event.y});
                    const PinId targetPinId =
                        HitTestPin(layoutCache, canvasPos.x, canvasPos.y);
                    if (targetPinId != INVALID_ID
                        && graph.CanConnect(draggingLinkPinId, targetPinId)) {
                        undoStack.Execute(
                            std::make_unique<AddLinkCommand>(draggingLinkPinId, targetPinId),
                            graph);
                    }
                    draggingLinkPinId = INVALID_ID;
                }
                if (draggingPointLinkId != INVALID_ID) {
                    if (pointMoved) {
                        const Link* link = graph.FindLink(draggingPointLinkId);
                        if (link != nullptr && draggingPointIndex >= 0
                            && draggingPointIndex < static_cast<int>(link->points.size())) {
                            const LinkPoint& point =
                                link->points[static_cast<std::size_t>(draggingPointIndex)];
                            undoStack.Execute(
                                std::make_unique<MoveLinkPointCommand>(
                                    draggingPointLinkId, draggingPointIndex,
                                    pointFromX, pointFromY, point.x, point.y),
                                graph);
                        }
                    }
                    draggingPointLinkId = INVALID_ID;
                    draggingPointIndex = -1;
                }
                if (draggingNodes) {
                    EndNodeDrag(graph, undoStack);
                }
                if (draggingCommentId != INVALID_ID) {
                    EndCommentDrag(graph, undoStack);
                }
                if (resizingCommentId != INVALID_ID) {
                    EndCommentResize(graph, undoStack);
                }
                rubberBanding = false;
            }
            break;

        case EditorInputType::MouseMove:
            if (panning) {
                const float dx = event.x - lastMouseX;
                const float dy = event.y - lastMouseY;
                canvas.PanByScreenDelta(dx, dy);
                dragDistance += std::fabs(dx) + std::fabs(dy);
            } else if (draggingLinkPinId != INVALID_ID) {
                const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{event.x, event.y});
                linkDragCanvasX = canvasPos.x;
                linkDragCanvasY = canvasPos.y;
            } else if (draggingPointLinkId != INVALID_ID) {
                Link* link = graph.FindLink(draggingPointLinkId);
                if (link != nullptr && draggingPointIndex >= 0
                    && draggingPointIndex < static_cast<int>(link->points.size())) {
                    const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{event.x, event.y});
                    link->points[static_cast<std::size_t>(draggingPointIndex)].x = canvasPos.x;
                    link->points[static_cast<std::size_t>(draggingPointIndex)].y = canvasPos.y;
                    pointMoved = true;
                }
            } else if (draggingNodes) {
                // Live move of the whole selection; the undoable command
                // is pushed on release.
                const float dx = (event.x - lastMouseX) / canvas.GetZoom();
                const float dy = (event.y - lastMouseY) / canvas.GetZoom();
                for (NodeId nodeId : selectedNodes) {
                    Node* node = graph.FindNode(nodeId);
                    if (node != nullptr) {
                        node->x += dx;
                        node->y += dy;
                    }
                }
                nodeMoved = true;
            } else if (draggingCommentId != INVALID_ID) {
                const float dx = (event.x - lastMouseX) / canvas.GetZoom();
                const float dy = (event.y - lastMouseY) / canvas.GetZoom();
                CommentNode* comment = graph.FindComment(draggingCommentId);
                if (comment != nullptr) {
                    comment->x += dx;
                    comment->y += dy;
                }
                for (const NodeMove& move : commentContainedMoves) {
                    Node* node = graph.FindNode(move.nodeId);
                    if (node != nullptr) {
                        node->x += dx;
                        node->y += dy;
                    }
                }
                commentDragDistance += std::fabs(event.x - lastMouseX)
                                     + std::fabs(event.y - lastMouseY);
            } else if (resizingCommentId != INVALID_ID) {
                CommentNode* comment = graph.FindComment(resizingCommentId);
                if (comment != nullptr) {
                    comment->width += (event.x - lastMouseX) / canvas.GetZoom();
                    comment->height += (event.y - lastMouseY) / canvas.GetZoom();
                    if (comment->width < COMMENT_MIN_WIDTH) {
                        comment->width = COMMENT_MIN_WIDTH;
                    }
                    if (comment->height < COMMENT_MIN_HEIGHT) {
                        comment->height = COMMENT_MIN_HEIGHT;
                    }
                }
            } else if (rubberBanding) {
                const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{event.x, event.y});
                bandEndCanvasX = canvasPos.x;
                bandEndCanvasY = canvasPos.y;
                selectedNodes = HitTestNodesInRect(layoutCache,
                                                   bandStartCanvasX, bandStartCanvasY,
                                                   bandEndCanvasX, bandEndCanvasY);
            }
            lastMouseX = event.x;
            lastMouseY = event.y;
            break;

        case EditorInputType::MouseWheel:
            canvas.ZoomAt(Vec2{event.x, event.y}, std::pow(ZOOM_STEP, event.wheelDelta));
            break;

        case EditorInputType::KeyDown:
            if (IsEditingTitle()) {
                if (event.key == EditorKey::Enter) {
                    CommitTitleEdit(graph, undoStack);
                } else if (event.key == EditorKey::Escape) {
                    CancelTitleEdit();
                } else if (event.key == EditorKey::Backspace) {
                    while (!titleEditText.empty()
                           && (static_cast<unsigned char>(titleEditText.back()) & 0xC0) == 0x80) {
                        titleEditText.pop_back();
                    }
                    if (!titleEditText.empty()) {
                        titleEditText.pop_back();
                    }
                }
            }
            break;

        case EditorInputType::TextInput:
            if (IsEditingTitle() && titleEditText.size() < 64) {
                titleEditText += event.text;
            }
            break;
        }
        return false;
    }
};

// Creates a comment box: wrapping the selection (plus padding) when
// nodes are selected, else default-sized at the menu spawn position.
static void CreateComment(const ContextMenu& menu, NodeGraph& graph, UndoStack& undoStack,
                          const NodeLayoutCache& layoutCache,
                          const std::vector<NodeId>& selectedNodes)
{
    bool hasBounds = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    for (NodeId nodeId : selectedNodes) {
        const NodeLayout* layout = layoutCache.Find(nodeId);
        if (layout == nullptr) {
            continue;
        }
        if (!hasBounds) {
            minX = layout->x;
            minY = layout->y;
            maxX = layout->x + layout->width;
            maxY = layout->y + layout->height;
            hasBounds = true;
        } else {
            minX = std::min(minX, layout->x);
            minY = std::min(minY, layout->y);
            maxX = std::max(maxX, layout->x + layout->width);
            maxY = std::max(maxY, layout->y + layout->height);
        }
    }

    float x = menu.GetSpawnCanvasX();
    float y = menu.GetSpawnCanvasY();
    float width = COMMENT_DEFAULT_WIDTH;
    float height = COMMENT_DEFAULT_HEIGHT;
    if (hasBounds) {
        x = minX - COMMENT_WRAP_PADDING;
        y = minY - COMMENT_WRAP_PADDING - COMMENT_TITLE_HEIGHT;
        width = (maxX - minX) + COMMENT_WRAP_PADDING * 2.0f;
        height = (maxY - minY) + COMMENT_WRAP_PADDING * 2.0f + COMMENT_TITLE_HEIGHT;
    }

    undoStack.Execute(std::make_unique<AddCommentCommand>("Comment", x, y, width, height), graph);
}

static void ProcessMenuAction(const ContextMenuAction& action, const ContextMenu& menu,
                              NodeGraph& graph, UndoStack& undoStack,
                              ClassEditorDialog& classDialog, float screenWidth, float screenHeight,
                              const NodeLayoutCache& layoutCache,
                              const std::vector<NodeId>& selectedNodes)
{
    if (action.type == ContextMenuAction::Type::OpenClassEditor) {
        classDialog.Open(screenWidth, screenHeight);
        return;
    }
    if (action.type == ContextMenuAction::Type::EditClass && action.nodeClass != nullptr) {
        classDialog.OpenForEdit(*action.nodeClass, screenWidth, screenHeight);
        return;
    }
    if (action.type == ContextMenuAction::Type::AddComment) {
        CreateComment(menu, graph, undoStack, layoutCache, selectedNodes);
        return;
    }
    if (action.type != ContextMenuAction::Type::CreateNode || action.nodeClass == nullptr) {
        return;
    }
    undoStack.Execute(
        std::make_unique<AddNodeCommand>(*action.nodeClass, menu.GetSpawnCanvasX(), menu.GetSpawnCanvasY()),
        graph);
}

// Registers (or, in edit mode, replaces) the class from a validated
// dialog submission and persists it to custom_nodes.json.
static void ProcessClassEditorAction(const ClassEditorAction& action, NodeGraph& graph)
{
    if (action.type != ClassEditorAction::Type::Submit) {
        return;
    }

    std::string error;

    if (action.editTarget != nullptr) {
        const std::string oldName = action.editTarget->GetName();
        if (!NodeClass::UpdateDynamic(action.editTarget, action.name, action.category,
                                      action.pins, action.properties)) {
            std::printf("class editor: builtin class cannot be edited\n");
            return;
        }
        graph.RebuildNodesOfClass(*action.editTarget);
        if (!UpdateNodeClassInFile("custom_nodes.json", oldName, action.name, action.category,
                                   action.pins, action.properties, error)) {
            std::printf("custom_nodes.json: %s\n", error.c_str());
        }
        return;
    }

    NodeClass::AdoptDynamic(
        std::make_unique<NodeClass>(action.name, action.category, action.pins, action.properties));

    if (!AppendNodeClassToFile("custom_nodes.json", action.name, action.category,
                               action.pins, action.properties, error)) {
        std::printf("custom_nodes.json: %s\n", error.c_str());
    }
}

static bool ContainsNodeId(const std::vector<NodeId>& nodeIds, NodeId nodeId)
{
    for (NodeId candidate : nodeIds) {
        if (candidate == nodeId) {
            return true;
        }
    }
    return false;
}

static void RenderComments(NVGcontext* vg, const Canvas& canvas, const NodeGraph& graph,
                           CommentId editingCommentId, const std::string& titleEditText)
{
    nvgSave(vg);
    nvgScale(vg, canvas.GetZoom(), canvas.GetZoom());
    nvgTranslate(vg, -canvas.GetPanX(), -canvas.GetPanY());

    for (const CommentNode& comment : graph.GetComments()) {
        DrawComment(vg, comment, comment.id == editingCommentId, titleEditText);
    }

    nvgRestore(vg);
}

static void RenderNodes(NVGcontext* vg, const Canvas& canvas,
                        const NodeGraph& graph, NodeLayoutCache& layoutCache,
                        const std::vector<NodeId>& selectedNodes,
                        PinId draggingLinkPinId, float linkDragCanvasX, float linkDragCanvasY)
{
    layoutCache.Clear();

    nvgSave(vg);
    nvgScale(vg, canvas.GetZoom(), canvas.GetZoom());
    nvgTranslate(vg, -canvas.GetPanX(), -canvas.GetPanY());

    // First pass: layouts (with pin connection state) so links can be
    // drawn beneath the node bodies.
    for (const Node& node : graph.GetNodes()) {
        NodeLayout layout = ComputeNodeLayout(vg, node);
        for (PinLayout& pinLayout : layout.pins) {
            pinLayout.connected = graph.IsPinConnected(pinLayout.pinId);
        }
        layoutCache.Add(layout);
    }

    DrawLinks(vg, graph, layoutCache);

    for (const Node& node : graph.GetNodes()) {
        const NodeLayout* layout = layoutCache.Find(node.id);
        if (layout != nullptr) {
            DrawNode(vg, node, *layout, ContainsNodeId(selectedNodes, node.id));
        }
    }

    if (draggingLinkPinId != INVALID_ID) {
        DrawDraggingLink(vg, layoutCache, draggingLinkPinId, linkDragCanvasX, linkDragCanvasY);
    }

    nvgRestore(vg);
}

// Registers extra node classes from custom_nodes.json in the working
// directory. Missing file is fine (builtins only); malformed entries are
// reported and skipped.
static void LoadCustomNodeClasses()
{
    const char* path = "custom_nodes.json";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return;
    }

    std::vector<std::string> errors;
    const int loadedCount = LoadNodeClassesFromFile(path, errors);
    for (const std::string& error : errors) {
        std::printf("custom_nodes.json: %s\n", error.c_str());
    }
    if (loadedCount > 0) {
        std::printf("custom_nodes.json: loaded %d node class(es)\n", loadedCount);
    }
}

static const char* EDITOR_SETTINGS_PATH = "editor_settings.json";

// Directory holding auto-saved graphs of untitled documents so they
// survive restarts without an explicit save.
static const char* SESSION_DIR = ".gau_session";

// One open graph file: its own model, undo history and view.
struct Document
{
    NodeGraph graph;
    UndoStack undoStack;
    Canvas canvas;
    // Empty for untitled documents.
    std::string filePath;
    std::string displayName = "Untitled";
    // Undo depth at the last successful save; differing depth = dirty.
    std::size_t savedUndoDepth = 0;

    bool IsDirty() const { return undoStack.GetDepth() != savedUndoDepth; }
};

static void ApplyDefaultZoom(Document& doc, const PlatformWindow& window)
{
    doc.canvas.ZoomAt(Vec2{static_cast<float>(window.GetWidth()) * 0.5f,
                           static_cast<float>(window.GetHeight()) * 0.5f},
                      DEFAULT_CANVAS_ZOOM);
}

static int AddNewDocument(std::vector<std::unique_ptr<Document>>& documents,
                          const PlatformWindow& window)
{
    documents.push_back(std::make_unique<Document>());
    ApplyDefaultZoom(*documents.back(), window);
    return static_cast<int>(documents.size()) - 1;
}

// Writes the document to its known path and refreshes the dirty marker.
static bool SaveDocumentToKnownPath(Document& doc)
{
    std::string error;
    if (!SaveGraphToFile(doc.graph, doc.filePath, error)) {
        std::printf("save failed: %s\n", error.c_str());
        return false;
    }
    doc.savedUndoDepth = doc.undoStack.GetDepth();
    std::printf("saved: %s\n", doc.filePath.c_str());
    return true;
}

// Saves to the document's path, or opens the save dialog for untitled
// documents.
static void SaveDocument(Document& doc, PlatformWindow& window)
{
    if (doc.filePath.empty()) {
        ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::SaveGraph);
        return;
    }
    SaveDocumentToKnownPath(doc);
}

// Saves synchronously, pumping events while an untitled document's save
// dialog is open. Uses a local event buffer so callers iterating the
// frame's event list are not invalidated. Returns false when the user
// cancelled (or quit).
static bool SaveDocumentBlocking(Document& doc, PlatformWindow& window)
{
    if (!doc.filePath.empty()) {
        return SaveDocumentToKnownPath(doc);
    }

    ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::SaveGraph);
    std::vector<EditorInputEvent> waitEvents;
    FileDialogResult result;
    for (;;) {
        if (!window.PumpEvents(waitEvents)) {
            return false;
        }
        if (PollFileDialogResult(result)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!result.accepted || result.type != FileDialogType::SaveGraph) {
        return false;
    }

    std::filesystem::path path(result.path);
    if (path.extension().empty()) {
        path += ".json";
    }
    doc.filePath = path.string();
    doc.displayName = path.filename().string();
    return SaveDocumentToKnownPath(doc);
}

static bool UntitledHasContent(const Document& doc)
{
    return doc.filePath.empty()
        && (!doc.graph.GetNodes().empty() || !doc.graph.GetComments().empty());
}

// Asks about unsaved changes when needed. discardsDocument is true for
// tab closes (the document is really lost); at quit untitled documents
// are auto-persisted to the session, so only titled dirty documents
// prompt. Returns false when closing must be aborted.
static bool ConfirmDocumentClose(Document& doc, PlatformWindow& window, bool discardsDocument)
{
    const bool needsPrompt = discardsDocument
                                 ? (doc.IsDirty() || UntitledHasContent(doc))
                                 : (doc.IsDirty() && !doc.filePath.empty());
    if (!needsPrompt) {
        return true;
    }
    switch (ShowConfirmSaveDialog(window.GetSDLWindow(), doc.displayName)) {
    case ConfirmSaveResult::Save:
        return SaveDocumentBlocking(doc, window);
    case ConfirmSaveResult::Discard:
        return true;
    case ConfirmSaveResult::Cancel:
        break;
    }
    return false;
}

// Inline rename of a tab title, started by clicking the active tab.
struct TabRenameState
{
    // Document index being renamed, -1 when inactive.
    int docIndex = -1;
    std::string text;

    bool IsActive() const { return docIndex >= 0; }
};

static void PopLastUTF8Character(std::string& text)
{
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

static std::string TrimSpaces(const std::string& text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && text[begin] == ' ') {
        ++begin;
    }
    while (end > begin && text[end - 1] == ' ') {
        --end;
    }
    return text.substr(begin, end - begin);
}

// Applies the rename: saved documents are renamed on disk (keeping a
// .json extension); untitled documents only change their display name.
static void CommitTabRename(TabRenameState& rename,
                            std::vector<std::unique_ptr<Document>>& documents)
{
    if (!rename.IsActive() || rename.docIndex >= static_cast<int>(documents.size())) {
        rename = TabRenameState();
        return;
    }
    Document& doc = *documents[static_cast<std::size_t>(rename.docIndex)];
    const std::string newName = TrimSpaces(rename.text);
    rename = TabRenameState();

    if (newName.empty() || newName == doc.displayName) {
        return;
    }
    if (doc.filePath.empty()) {
        doc.displayName = newName;
        return;
    }

    const std::filesystem::path oldPath(doc.filePath);
    std::filesystem::path newPath = oldPath.parent_path() / newName;
    if (newPath.extension().empty()) {
        newPath += ".json";
    }
    if (newPath == oldPath) {
        return;
    }
    std::error_code ec;
    if (std::filesystem::exists(newPath, ec)) {
        std::printf("rename failed: file already exists: %s\n", newPath.string().c_str());
        return;
    }
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        std::printf("rename failed: %s\n", ec.message().c_str());
        return;
    }
    doc.filePath = newPath.string();
    doc.displayName = newPath.filename().string();
    std::printf("renamed: %s\n", doc.filePath.c_str());
}

// Parses a property-panel edit and applies it as an undoable command.
static void ApplyPropertyPanelAction(const PropertyPanelAction& action, const Node& node,
                                     NodeGraph& graph, UndoStack& undoStack)
{
    if (action.type != PropertyPanelAction::Type::SetProperty || action.propertyIndex < 0
        || action.propertyIndex >= static_cast<int>(node.nodeClass->GetProperties().size())) {
        return;
    }
    const PropertyDef& def =
        node.nodeClass->GetProperties()[static_cast<std::size_t>(action.propertyIndex)];

    PropertyValue newValue;
    std::string error;
    if (!ParsePropertyValueText(def, action.text, newValue, error)) {
        std::printf("property '%s': %s\n", def.name.c_str(), error.c_str());
        return;
    }
    undoStack.Execute(
        std::make_unique<SetNodePropertyCommand>(node.id, action.propertyIndex,
                                                 std::move(newValue)),
        graph);
}

// Prompts for every dirty document before quitting; documents stay open
// so the session file still records them. Returns false to abort quit.
static bool ConfirmQuit(std::vector<std::unique_ptr<Document>>& documents,
                        PlatformWindow& window)
{
    for (std::unique_ptr<Document>& doc : documents) {
        if (!ConfirmDocumentClose(*doc, window, false)) {
            return false;
        }
    }
    return true;
}

// Loads a graph file into a new document tab. Returns false when the
// file could not be read/parsed (per-entry problems are only reported).
static bool OpenDocumentFromPath(const std::string& filePath,
                                 std::vector<std::unique_ptr<Document>>& documents,
                                 const PlatformWindow& window)
{
    auto doc = std::make_unique<Document>();
    std::vector<std::string> errors;
    const bool loaded = LoadGraphFromFile(doc->graph, filePath, errors);
    for (const std::string& error : errors) {
        std::printf("open: %s\n", error.c_str());
    }
    if (!loaded) {
        return false;
    }
    const std::filesystem::path path(filePath);
    doc->filePath = path.string();
    doc->displayName = path.filename().string();
    ApplyDefaultZoom(*doc, window);
    documents.push_back(std::move(doc));
    return true;
}

// Applies a completed native file dialog result to the document list.
static void ProcessFileDialogResult(const FileDialogResult& result,
                                    std::vector<std::unique_ptr<Document>>& documents,
                                    int& activeDocIndex, const PlatformWindow& window,
                                    CanvasController& controller)
{
    if (!result.accepted) {
        return;
    }

    if (result.type == FileDialogType::SaveGraph) {
        std::filesystem::path path(result.path);
        if (path.extension().empty()) {
            path += ".json";
        }
        Document& doc = *documents[static_cast<std::size_t>(activeDocIndex)];
        doc.filePath = path.string();
        doc.displayName = path.filename().string();
        SaveDocumentToKnownPath(doc);
        return;
    }

    if (OpenDocumentFromPath(result.path, documents, window)) {
        activeDocIndex = static_cast<int>(documents.size()) - 1;
        controller = CanvasController();
    }
}

// Reopens the files from the previous session; returns the index of the
// previously active document (0 when none matches).
static int RestoreSessionDocuments(const EditorSettings& settings,
                                   std::vector<std::unique_ptr<Document>>& documents,
                                   const PlatformWindow& window)
{
    int activeIndex = 0;
    for (const OpenFileEntry& openFile : settings.openFiles) {
        std::error_code ec;
        if (!std::filesystem::exists(openFile.path, ec)) {
            std::printf("session restore: file missing, skipped: %s\n", openFile.path.c_str());
            continue;
        }
        if (!OpenDocumentFromPath(openFile.path, documents, window)) {
            continue;
        }
        Document& doc = *documents.back();
        if (openFile.untitled) {
            // Restore as untitled: the backing file is session-managed.
            doc.filePath.clear();
            doc.displayName = openFile.displayName.empty() ? "Untitled" : openFile.displayName;
        }
        if (openFile.zoom > 0.0f) {
            doc.canvas.SetView(openFile.panX, openFile.panY, openFile.zoom);
        }
        if (!settings.activeFilePath.empty() && openFile.path == settings.activeFilePath) {
            activeIndex = static_cast<int>(documents.size()) - 1;
        }
    }
    return activeIndex;
}

// Records the open documents for the next session. Untitled documents
// with content are auto-saved into SESSION_DIR backing files so they
// survive restarts without an explicit save.
static void CollectSessionDocuments(EditorSettings& settings,
                                    const std::vector<std::unique_ptr<Document>>& documents,
                                    int activeDocIndex)
{
    std::error_code ec;
    std::filesystem::create_directories(SESSION_DIR, ec);
    // Drop backing files from the previous session; live ones are
    // rewritten below.
    if (std::filesystem::is_directory(SESSION_DIR, ec)) {
        for (const auto& dirEntry : std::filesystem::directory_iterator(SESSION_DIR, ec)) {
            const std::string fileName = dirEntry.path().filename().string();
            if (fileName.rfind("untitled_", 0) == 0 && dirEntry.path().extension() == ".json") {
                std::filesystem::remove(dirEntry.path(), ec);
            }
        }
    }

    settings.openFiles.clear();
    settings.activeFilePath.clear();
    int untitledCounter = 0;

    for (int i = 0; i < static_cast<int>(documents.size()); ++i) {
        const Document& doc = *documents[static_cast<std::size_t>(i)];
        OpenFileEntry entry;

        if (doc.filePath.empty()) {
            if (!UntitledHasContent(doc)) {
                continue;
            }
            ++untitledCounter;
            const std::filesystem::path backingPath =
                std::filesystem::path(SESSION_DIR)
                / ("untitled_" + std::to_string(untitledCounter) + ".json");
            std::string error;
            if (!SaveGraphToFile(doc.graph, backingPath.string(), error)) {
                std::printf("session save failed: %s\n", error.c_str());
                continue;
            }
            entry.path = backingPath.string();
            entry.untitled = true;
            entry.displayName = doc.displayName;
        } else {
            entry.path = doc.filePath;
        }

        entry.panX = doc.canvas.GetPanX();
        entry.panY = doc.canvas.GetPanY();
        entry.zoom = doc.canvas.GetZoom();
        if (i == activeDocIndex) {
            settings.activeFilePath = entry.path;
        }
        settings.openFiles.push_back(std::move(entry));
    }
}

// Handles a left click on the tab bar. Returns true when consumed.
static bool ProcessTabBarClick(const EditorInputEvent& event, float screenWidth,
                               std::vector<std::unique_ptr<Document>>& documents,
                               int& activeDocIndex, PlatformWindow& window,
                               CanvasController& controller, TabRenameState& tabRename)
{
    const TabBarHit hit = HitTestTabBar(event.x, event.y,
                                        static_cast<int>(documents.size()), screenWidth);
    switch (hit.kind) {
    case TabBarHit::Kind::None:
        break;
    case TabBarHit::Kind::Tab:
        if (hit.tabIndex != activeDocIndex) {
            activeDocIndex = hit.tabIndex;
            controller = CanvasController();
        } else {
            // Clicking the already-active tab starts inline renaming.
            tabRename.docIndex = hit.tabIndex;
            tabRename.text =
                documents[static_cast<std::size_t>(hit.tabIndex)]->displayName;
        }
        break;
    case TabBarHit::Kind::CloseTab:
        if (!ConfirmDocumentClose(*documents[static_cast<std::size_t>(hit.tabIndex)],
                                  window, true)) {
            break;
        }
        documents.erase(documents.begin() + hit.tabIndex);
        if (documents.empty()) {
            AddNewDocument(documents, window);
        }
        if (activeDocIndex >= static_cast<int>(documents.size())) {
            activeDocIndex = static_cast<int>(documents.size()) - 1;
        }
        controller = CanvasController();
        break;
    case TabBarHit::Kind::NewTab:
        activeDocIndex = AddNewDocument(documents, window);
        controller = CanvasController();
        break;
    case TabBarHit::Kind::Open:
        ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::OpenGraph);
        break;
    case TabBarHit::Kind::Save:
        SaveDocument(*documents[static_cast<std::size_t>(activeDocIndex)], window);
        break;
    case TabBarHit::Kind::SaveAs:
        ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::SaveGraph);
        break;
    }
    return true;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    LoadCustomNodeClasses();

    EditorSettings settings;
    settings.LoadFromFile(EDITOR_SETTINGS_PATH);

    PlatformWindow window;
    if (!window.Init("GAU Node Editor", 1280, 720)) {
        return 1;
    }

    NVGcontext* vg = CreatePlatformNVGContext();
    if (vg == nullptr) {
        window.Shutdown();
        return 1;
    }

    CanvasController controller;
    NodeLayoutCache layoutCache;
    ContextMenu contextMenu;
    ClassEditorDialog classDialog;
    PropertyPanel propertyPanel;
    ActionMenu actionMenu;
    NodeId actionTargetNodeId = INVALID_ID;
    CommentId actionTargetCommentId = INVALID_ID;
    TabRenameState tabRename;
    std::vector<EditorInputEvent> events;

    std::vector<std::unique_ptr<Document>> documents;
    int activeDocIndex = RestoreSessionDocuments(settings, documents, window);
    if (documents.empty()) {
        activeDocIndex = AddNewDocument(documents, window);
    }

    contextMenu.SetCollapsedCategories(settings.collapsedCategories);

    for (;;) {
        if (!window.PumpEvents(events)) {
            if (ConfirmQuit(documents, window)) {
                break;
            }
            continue;
        }

        const float screenWidth = static_cast<float>(window.GetWidth());
        const float screenHeight = static_cast<float>(window.GetHeight());

        for (const EditorInputEvent& event : events) {
            Document& doc = *documents[static_cast<std::size_t>(activeDocIndex)];

            if (classDialog.IsOpen()) {
                const ClassEditorAction action = classDialog.HandleEvent(event);
                ProcessClassEditorAction(action, doc.graph);
                continue;
            }

            if (actionMenu.IsOpen()) {
                const ActionMenuResult result = actionMenu.HandleEvent(event);
                if (result.type == ActionMenuResult::Type::Selected && result.itemIndex == 0) {
                    if (actionTargetNodeId != INVALID_ID) {
                        std::vector<NodeId> deleteIds;
                        if (controller.IsSelected(actionTargetNodeId)) {
                            deleteIds = controller.selectedNodes;
                        } else {
                            deleteIds.push_back(actionTargetNodeId);
                        }
                        doc.undoStack.Execute(
                            std::make_unique<DeleteNodesCommand>(std::move(deleteIds)),
                            doc.graph);
                        controller.selectedNodes.clear();
                    } else if (actionTargetCommentId != INVALID_ID) {
                        if (controller.editingCommentId == actionTargetCommentId) {
                            controller.CancelTitleEdit();
                        }
                        doc.undoStack.Execute(
                            std::make_unique<DeleteCommentCommand>(actionTargetCommentId),
                            doc.graph);
                    }
                }
                continue;
            }

            if (contextMenu.IsOpen()) {
                const ContextMenuAction action = contextMenu.HandleEvent(event);
                ProcessMenuAction(action, contextMenu, doc.graph, doc.undoStack,
                                  classDialog, screenWidth, screenHeight,
                                  layoutCache, controller.selectedNodes);
                continue;
            }

            // Inline tab renaming consumes keyboard input; any click
            // commits it before normal handling continues.
            if (tabRename.IsActive()) {
                if (event.type == EditorInputType::KeyDown) {
                    if (event.key == EditorKey::Enter) {
                        CommitTabRename(tabRename, documents);
                    } else if (event.key == EditorKey::Escape) {
                        tabRename = TabRenameState();
                    } else if (event.key == EditorKey::Backspace) {
                        PopLastUTF8Character(tabRename.text);
                    }
                    continue;
                }
                if (event.type == EditorInputType::TextInput) {
                    if (tabRename.text.size() < 60) {
                        tabRename.text += event.text;
                    }
                    continue;
                }
                if (event.type == EditorInputType::MouseDown) {
                    const int renamingIndex = tabRename.docIndex;
                    CommitTabRename(tabRename, documents);
                    // Clicking the same tab again just ends the rename.
                    if (event.y < TAB_BAR_HEIGHT) {
                        const TabBarHit hit =
                            HitTestTabBar(event.x, event.y,
                                          static_cast<int>(documents.size()), screenWidth);
                        if (hit.kind == TabBarHit::Kind::Tab
                            && hit.tabIndex == renamingIndex) {
                            continue;
                        }
                    }
                }
            }

            // Inline comment renaming consumes keyboard input first.
            if (controller.IsEditingTitle()
                && (event.type == EditorInputType::KeyDown
                    || event.type == EditorInputType::TextInput)) {
                controller.HandleEvent(event, doc.canvas, doc.graph, layoutCache, doc.undoStack);
                continue;
            }

            // Property panel of the selected node (dockable).
            {
                const Node* selectedNode = controller.selectedNodes.empty()
                                               ? nullptr
                                               : doc.graph.FindNode(controller.selectedNodes.back());
                propertyPanel.SetTarget(selectedNode);
                if (selectedNode != nullptr) {
                    PropertyPanelAction panelAction;
                    const bool consumed = propertyPanel.HandleEvent(
                        event, selectedNode, screenWidth, screenHeight, panelAction);
                    ApplyPropertyPanelAction(panelAction, *selectedNode, doc.graph,
                                             doc.undoStack);
                    if (consumed) {
                        continue;
                    }
                }
            }

            // Clicks in the tab bar never reach the canvas.
            if (event.type == EditorInputType::MouseDown && event.y < TAB_BAR_HEIGHT) {
                if (event.button == EditorMouseButton::Left) {
                    ProcessTabBarClick(event, screenWidth, documents, activeDocIndex,
                                       window, controller, tabRename);
                    // Blocking dialogs may have re-pumped the event list;
                    // stop iterating this frame's events.
                    break;
                }
                continue;
            }

            if (event.type == EditorInputType::KeyDown) {
                if (event.key == EditorKey::Undo) {
                    doc.undoStack.Undo(doc.graph);
                    continue;
                }
                if (event.key == EditorKey::Redo) {
                    doc.undoStack.Redo(doc.graph);
                    continue;
                }
                if (event.key == EditorKey::Tab) {
                    const float mouseX = controller.lastMouseX;
                    const float mouseY = controller.lastMouseY;
                    const Vec2 canvasPos = doc.canvas.ScreenToCanvas(Vec2{mouseX, mouseY});
                    contextMenu.Open(mouseX, mouseY, canvasPos.x, canvasPos.y,
                                     screenWidth, screenHeight);
                    continue;
                }
                if (event.key == EditorKey::Delete) {
                    if (!controller.selectedNodes.empty()) {
                        doc.undoStack.Execute(
                            std::make_unique<DeleteNodesCommand>(controller.selectedNodes),
                            doc.graph);
                        controller.selectedNodes.clear();
                    }
                    continue;
                }
            }

            if (controller.HandleEvent(event, doc.canvas, doc.graph, layoutCache, doc.undoStack)) {
                // Right click: action menu on a node/comment, creation
                // menu on empty canvas.
                const Vec2 canvasPos = doc.canvas.ScreenToCanvas(Vec2{event.x, event.y});
                const NodeId hitNodeId = HitTestNode(layoutCache, canvasPos.x, canvasPos.y);
                if (hitNodeId != INVALID_ID) {
                    actionTargetNodeId = hitNodeId;
                    actionTargetCommentId = INVALID_ID;
                    actionMenu.Open(event.x, event.y, {"Delete"},
                                    screenWidth, screenHeight);
                    continue;
                }
                const CommentId hitCommentId =
                    HitTestCommentTitle(doc.graph, canvasPos.x, canvasPos.y);
                if (hitCommentId != INVALID_ID) {
                    actionTargetNodeId = INVALID_ID;
                    actionTargetCommentId = hitCommentId;
                    actionMenu.Open(event.x, event.y, {"Delete Comment"},
                                    screenWidth, screenHeight);
                    continue;
                }
                contextMenu.Open(event.x, event.y, canvasPos.x, canvasPos.y,
                                 screenWidth, screenHeight);
            }
        }

        FileDialogResult dialogResult;
        while (PollFileDialogResult(dialogResult)) {
            ProcessFileDialogResult(dialogResult, documents, activeDocIndex, window, controller);
        }

        Document& doc = *documents[static_cast<std::size_t>(activeDocIndex)];

        window.BeginFrame(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);

        nvgBeginFrame(vg, screenWidth, screenHeight, window.GetPixelRatio());
        DrawGrid(vg, doc.canvas, screenWidth, screenHeight);
        RenderComments(vg, doc.canvas, doc.graph, controller.editingCommentId,
                       controller.titleEditText);
        RenderNodes(vg, doc.canvas, doc.graph, layoutCache, controller.selectedNodes,
                    controller.draggingLinkPinId,
                    controller.linkDragCanvasX, controller.linkDragCanvasY);
        if (controller.rubberBanding) {
            DrawRubberBand(vg, doc.canvas,
                           controller.bandStartCanvasX, controller.bandStartCanvasY,
                           controller.bandEndCanvasX, controller.bandEndCanvasY);
        }

        std::vector<std::string> tabNames;
        for (const std::unique_ptr<Document>& document : documents) {
            tabNames.push_back(document->IsDirty() ? document->displayName + "*"
                                                   : document->displayName);
        }
        DrawTabBar(vg, tabNames, activeDocIndex, screenWidth,
                   tabRename.docIndex, tabRename.text);

        const Node* panelNode = controller.selectedNodes.empty()
                                    ? nullptr
                                    : doc.graph.FindNode(controller.selectedNodes.back());
        propertyPanel.SetTarget(panelNode);
        if (panelNode != nullptr) {
            DrawPropertyPanel(vg, propertyPanel, *panelNode, screenWidth);
        }

        DrawContextMenu(vg, contextMenu);
        DrawActionMenu(vg, actionMenu);
        DrawClassEditorDialog(vg, classDialog, screenWidth, screenHeight);
        nvgEndFrame(vg);

        window.EndFrame();
    }

    settings.collapsedCategories = contextMenu.GetCollapsedCategories();
    CollectSessionDocuments(settings, documents, activeDocIndex);
    if (!settings.SaveToFile(EDITOR_SETTINGS_PATH)) {
        std::printf("editor_settings.json: failed to save settings\n");
    }

    DestroyPlatformNVGContext(vg);
    window.Shutdown();
    return 0;
}
