#include "platform/PlatformWindow.h"
#include "platform/PlatformNVG.h"
#include "platform/PlatformClipboard.h"
#include "platform/PlatformConsole.h"
#include "platform/PlatformFileDialog.h"
#include "platform/PlatformProcess.h"
#include "interaction/FunctionEditorDialog.h"
#include "render/FunctionEditorRenderer.h"
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
#include "model/GraphClipboard.h"
#include "model/NodeClassLoader.h"
#include "model/EditorSettings.h"
#include "model/GraphSerializer.h"
#include "model/UndoStack.h"
#include "model/GraphCommands.h"
#include "interaction/EditorInputEvent.h"
#include "interaction/ContextMenu.h"
#include "interaction/ClassEditorDialog.h"
#include "exec/ExecEngine.h"
#include "exec/WasmApiHeader.h"
#include "exec/WasmRuntime.h"
#include "interaction/ActionMenu.h"
#include "interaction/HitTest.h"
#include "interaction/NodeAlign.h"
#include "interaction/LogPanel.h"
#include "render/LogPanelRenderer.h"
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
#include <fstream>
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
    // Selection to keep when a shift+rubber-band extends the current
    // selection; empty for a plain rubber-band.
    std::vector<NodeId> bandBaseSelection;
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

    void DeselectNode(NodeId nodeId)
    {
        for (std::size_t i = 0; i < selectedNodes.size(); ++i) {
            if (selectedNodes[i] == nodeId) {
                selectedNodes.erase(selectedNodes.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
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
                    if (event.ctrl) {
                        // Ctrl-click toggles the node in the selection.
                        // Deselecting does not start a drag.
                        if (IsSelected(hitNodeId)) {
                            DeselectNode(hitNodeId);
                            break;
                        }
                        selectedNodes.push_back(hitNodeId);
                        BeginNodeDrag(graph);
                        break;
                    }
                    if (event.shift) {
                        // Shift-click extends the selection.
                        if (!IsSelected(hitNodeId)) {
                            selectedNodes.push_back(hitNodeId);
                        }
                        BeginNodeDrag(graph);
                        break;
                    }
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
                // Shift keeps the current selection and adds the banded
                // nodes to it; a plain band replaces the selection.
                if (event.shift) {
                    bandBaseSelection = selectedNodes;
                } else {
                    bandBaseSelection.clear();
                    selectedNodes.clear();
                }
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
                std::vector<NodeId> banded =
                    HitTestNodesInRect(layoutCache, bandStartCanvasX, bandStartCanvasY,
                                       bandEndCanvasX, bandEndCanvasY);
                selectedNodes = bandBaseSelection;
                for (NodeId nodeId : banded) {
                    if (!IsSelected(nodeId)) {
                        selectedNodes.push_back(nodeId);
                    }
                }
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
    if (action.type == ClassEditorAction::Type::SubmitType) {
        // A rename drops the old entry (registry + file) before saving.
        if (!action.typeEditOldName.empty() && action.typeEditOldName != action.userType.name) {
            UserTypeRegistry::Remove(action.typeEditOldName);
            std::string removeError;
            if (!RemoveUserTypeFromFile("custom_nodes.json", action.typeEditOldName, removeError)) {
                std::printf("custom_nodes.json: %s\n", removeError.c_str());
            }
        }
        UserTypeRegistry::Register(action.userType);
        std::string typeError;
        if (!SaveUserTypeToFile("custom_nodes.json", action.userType, typeError)) {
            std::printf("custom_nodes.json: %s\n", typeError.c_str());
        }
        return;
    }
    if (action.type != ClassEditorAction::Type::Submit) {
        return;
    }

    std::string error;

    if (action.editTarget != nullptr) {
        const std::string oldName = action.editTarget->GetName();
        // The dialog does not edit the exec binding; preserve it.
        const std::string execFnName = action.editTarget->GetExecFnName();
        if (!NodeClass::UpdateDynamic(action.editTarget, action.name, action.category,
                                      action.pins, action.properties, execFnName)) {
            std::printf("class editor: builtin class cannot be edited\n");
            return;
        }
        graph.RebuildNodesOfClass(*action.editTarget);
        if (!UpdateNodeClassInFile("custom_nodes.json", oldName, action.name, action.category,
                                   action.pins, action.properties, execFnName, error)) {
            std::printf("custom_nodes.json: %s\n", error.c_str());
        }
        return;
    }

    NodeClass::AdoptDynamic(
        std::make_unique<NodeClass>(action.name, action.category, action.pins, action.properties));

    if (!AppendNodeClassToFile("custom_nodes.json", action.name, action.category,
                               action.pins, action.properties, std::string(), error)) {
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
                        PinId draggingLinkPinId, float linkDragCanvasX, float linkDragCanvasY,
                        const PinValueCache& previewValues)
{
    layoutCache.Clear();

    nvgSave(vg);
    nvgScale(vg, canvas.GetZoom(), canvas.GetZoom());
    nvgTranslate(vg, -canvas.GetPanX(), -canvas.GetPanY());

    // First pass: layouts (with pin connection state) so links can be
    // drawn beneath the node bodies.
    for (const Node& node : graph.GetNodes()) {
        NodeLayout layout = ComputeNodeLayout(vg, node, graph, previewValues);
        for (PinLayout& pinLayout : layout.pins) {
            pinLayout.connected = graph.IsPinConnected(pinLayout.pinId);
        }
        layoutCache.Add(layout);
    }

    DrawLinks(vg, graph, layoutCache);

    for (const Node& node : graph.GetNodes()) {
        const NodeLayout* layout = layoutCache.Find(node.id);
        if (layout != nullptr) {
            DrawNode(vg, node, *layout, ContainsNodeId(selectedNodes, node.id),
                     graph, previewValues);
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

// Custom wasm node functions: compiled modules and their C sources.
static const char* WASM_MODULE_DIR = "wasm";
static const char* WASM_SOURCE_DIR = "wasm_src";

static void LoadWasmModules()
{
    std::vector<std::string> errors;
    const int loadedCount =
        WasmRuntime::Instance().LoadModulesFromDirectory(WASM_MODULE_DIR, errors);
    for (const std::string& error : errors) {
        std::printf("wasm: %s\n", error.c_str());
    }
    if (loadedCount > 0) {
        std::printf("wasm: loaded %d module(s)\n", loadedCount);
    }
}

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

    // Cached data-preview values for node body display, refreshed only
    // when the undo revision changes. Sentinel forces a first compute.
    PinValueCache previewValues;
    std::uint64_t previewRevision = UINT64_MAX;

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

// Locates a clang with the wasm32 target: explicit setting first, then
// the bundled toolchain shipped next to the app, then the official
// LLVM install, then PATH. Bundling clang.exe + wasm-ld.exe under
// tools/llvm/bin makes distributed builds self-contained.
static std::string FindClangForWasm(const EditorSettings& settings)
{
    if (!settings.clangPath.empty()) {
        return settings.clangPath;
    }
    std::error_code ec;
    const char* candidates[] = {
        "tools/llvm/bin/clang.exe",
        "tools/clang.exe",
        "C:/Program Files/LLVM/bin/clang.exe",
    };
    for (const char* candidate : candidates) {
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return "clang";
}

static bool IsValidFunctionName(const std::string& name)
{
    if (name.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        const bool digit = (c >= '0' && c <= '9');
        if (!(alpha || (i > 0 && digit))) {
            return false;
        }
    }
    return true;
}

// Parses node interface directives from a wasm function source:
//   // @node category=Pure
//   // @in ax:float ay:float
//   // @out x:float
// Pin tokens are name:type; "_" as a name means unnamed (exec pins).
// Returns true when at least one pin directive was found.
static bool ParseWasmNodeDirectives(const std::string& source, std::vector<PinDef>& outPins,
                                    std::string& outCategory)
{
    outPins.clear();
    outCategory = "Function";
    bool anyPins = false;

    std::size_t lineStart = 0;
    while (lineStart <= source.size()) {
        std::size_t lineEnd = source.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = source.size();
        }
        std::string line = source.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        std::size_t position = line.find("// @");
        if (position == std::string::npos) {
            continue;
        }
        line = line.substr(position + 3);

        PinDirection direction = PinDirection::Input;
        bool pinLine = false;
        if (line.rfind("@node", 0) == 0) {
            const std::size_t categoryPos = line.find("category=");
            if (categoryPos != std::string::npos) {
                std::string category = line.substr(categoryPos + 9);
                const std::size_t spacePos = category.find(' ');
                if (spacePos != std::string::npos) {
                    category = category.substr(0, spacePos);
                }
                if (!category.empty()) {
                    outCategory = category;
                }
            }
            continue;
        } else if (line.rfind("@in", 0) == 0) {
            direction = PinDirection::Input;
            line = line.substr(3);
            pinLine = true;
        } else if (line.rfind("@out", 0) == 0) {
            direction = PinDirection::Output;
            line = line.substr(4);
            pinLine = true;
        }
        if (!pinLine) {
            continue;
        }

        std::string token;
        line += ' ';
        for (char c : line) {
            if (c == ' ' || c == '\t' || c == '\r') {
                if (!token.empty()) {
                    const std::size_t colonPos = token.find(':');
                    if (colonPos != std::string::npos) {
                        PinDef pin;
                        pin.direction = direction;
                        pin.name = token.substr(0, colonPos);
                        if (pin.name == "_") {
                            pin.name.clear();
                        }
                        if (PinTypeFromString(token.substr(colonPos + 1), pin.type)) {
                            outPins.push_back(std::move(pin));
                            anyPins = true;
                        }
                    }
                    token.clear();
                }
            } else {
                token += c;
            }
        }
    }
    return anyPins;
}

// Creates or updates the node class driven by a wasm function's
// directives and persists it to custom_nodes.json.
static void RegisterWasmNodeClass(const std::string& name, const std::string& category,
                                  std::vector<PinDef> pins,
                                  std::vector<std::unique_ptr<Document>>& documents,
                                  std::vector<std::string>& logLines)
{
    const std::string execFn = "wasm:" + name;
    const NodeClass* existing = NodeClass::FindByName(name.c_str());
    std::string error;

    if (existing == nullptr) {
        NodeClass::AdoptDynamic(std::make_unique<NodeClass>(name, category, pins,
                                                            std::vector<PropertyDef>{}, execFn));
        if (!AppendNodeClassToFile("custom_nodes.json", name, category, pins, {}, execFn,
                                   error)) {
            std::printf("custom_nodes.json: %s\n", error.c_str());
        }
        logLines.push_back("node class registered: " + name);
        return;
    }
    if (!existing->IsDynamic()) {
        logLines.push_back("warning: builtin class '" + name + "' cannot be bound to wasm");
        return;
    }

    std::vector<PropertyDef> keptProperties = existing->GetProperties();
    NodeClass::UpdateDynamic(existing, name, category, pins, keptProperties, execFn);
    for (std::unique_ptr<Document>& doc : documents) {
        doc->graph.RebuildNodesOfClass(*existing);
    }
    if (!UpdateNodeClassInFile("custom_nodes.json", name, name, category, pins,
                               keptProperties, execFn, error)) {
        if (!AppendNodeClassToFile("custom_nodes.json", name, category, pins,
                                   keptProperties, execFn, error)) {
            std::printf("custom_nodes.json: %s\n", error.c_str());
        }
    }
    logLines.push_back("node class updated: " + name);
}

// Writes the edited source, compiles it to wasm with clang and reloads
// the modules. Node directives in the source create/update the class.
// Build output goes to the log panel; the dialog stays open with a
// status message on failure.
static void BuildWasmFunction(const FunctionEditorAction& action,
                              const EditorSettings& settings,
                              FunctionEditorDialog& functionEditor,
                              std::vector<std::unique_ptr<Document>>& documents,
                              std::vector<std::string>& logLines)
{
    if (!IsValidFunctionName(action.name)) {
        functionEditor.SetStatus("Invalid function name (identifier expected)");
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(WASM_SOURCE_DIR, ec);
    std::filesystem::create_directories(WASM_MODULE_DIR, ec);

    // Refresh the generated API header so runtime-added classes are
    // usable as types in the source.
    std::string headerError;
    if (!WriteWasmApiHeader(std::string(WASM_SOURCE_DIR) + "/gau_api.h", headerError)) {
        logLines.push_back("gau_api.h: " + headerError);
    }

    const std::string sourcePath = std::string(WASM_SOURCE_DIR) + "/" + action.name + ".cpp";
    {
        std::ofstream file(sourcePath, std::ios::binary);
        if (!file.is_open()) {
            functionEditor.SetStatus("Cannot write " + sourcePath);
            return;
        }
        file << action.source;
    }

    // Sources build as freestanding C++ (no exceptions/RTTI); exported
    // entry points must be extern "C" so their names stay unmangled.
    const std::string wasmPath = std::string(WASM_MODULE_DIR) + "/" + action.name + ".wasm";
    const std::string clang = FindClangForWasm(settings);
    const std::string command = "\"" + clang + "\" --target=wasm32 -nostdlib -O2"
                              + " -std=c++17 -fno-exceptions -fno-rtti"
                              + " -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined"
                              + " -o \"" + wasmPath + "\" \"" + sourcePath + "\"";

    logLines.push_back("--- wasm build: " + action.name + " ---");
    std::string output;
    int exitCode = -1;
    if (!RunCommandCaptured(command, output, exitCode)) {
        functionEditor.SetStatus("Failed to launch clang: " + clang);
        logLines.push_back("failed to launch: " + command);
        return;
    }

    std::string line;
    for (char c : output) {
        if (c == '\n') {
            if (!line.empty()) {
                logLines.push_back(line);
                std::printf("[wasm] %s\n", line.c_str());
            }
            line.clear();
        } else if (c != '\r') {
            line += c;
        }
    }
    if (!line.empty()) {
        logLines.push_back(line);
    }

    if (exitCode != 0) {
        if (output.find("wasm32") != std::string::npos
            && output.find("No available targets") != std::string::npos) {
            logLines.push_back("hint: install the official LLVM (llvm.org) and set"
                               " tools.clangPath in editor_settings.json");
        }
        functionEditor.SetStatus("Build failed (see Output panel)");
        return;
    }

    LoadWasmModules();

    std::vector<PinDef> directivePins;
    std::string directiveCategory;
    if (ParseWasmNodeDirectives(action.source, directivePins, directiveCategory)) {
        RegisterWasmNodeClass(action.name, directiveCategory, std::move(directivePins),
                              documents, logLines);
    } else {
        logLines.push_back("wasm build ok: no @in/@out directives; bind manually with"
                           " \"execFn\": \"wasm:" + action.name + "\"");
    }
    functionEditor.Close();
}

// Runs the active graph; log output fans out to the console, the log
// panel buffer, and later runtime views.
static void RunActiveGraph(const Document& doc, std::vector<std::string>& logLines)
{
    logLines.push_back("--- Run: " + doc.displayName + " ---");
    std::printf("[exec] --- Run: %s ---\n", doc.displayName.c_str());

    ExecEngine engine(doc.graph, [&logLines](const std::string& message) {
        std::printf("[exec] %s\n", message.c_str());
        logLines.push_back(message);
    });
    if (engine.Run()) {
        logLines.push_back("--- Done ---");
        std::printf("[exec] --- Done ---\n");
    }
}

// Pastes the clipboard at a canvas position and selects the new nodes.
static void PasteClipboardAt(const GraphClipboard& clipboard, float canvasX, float canvasY,
                             NodeGraph& graph, UndoStack& undoStack,
                             std::vector<NodeId>& outSelection)
{
    if (clipboard.IsEmpty()) {
        return;
    }
    auto command = std::make_unique<PasteClipboardCommand>(clipboard, canvasX, canvasY);
    const PasteClipboardCommand* rawCommand = command.get();
    if (undoStack.Execute(std::move(command), graph)) {
        outSelection.clear();
        for (NodeId nodeId : rawCommand->GetCreatedNodeIds()) {
            if (nodeId != INVALID_ID) {
                outSelection.push_back(nodeId);
            }
        }
    }
}

// Node action menu. Copy/Delete are always present; a multi-node
// selection also gets an "Align" item at index NODE_MENU_BASE_ITEMS whose
// hover submenu lists the alignment ops (parallel to AlignOp).
static constexpr int NODE_MENU_BASE_ITEMS = 2;
static const char* const ALIGN_MENU_LABELS[] = {
    "Align Left",
    "Align Center",
    "Align Right",
    "Align Top",
    "Align Middle",
    "Align Bottom",
    "Distribute Horizontally",
    "Distribute Vertically",
    "Straighten Connections",
};
static constexpr int ALIGN_OP_COUNT =
    static_cast<int>(sizeof(ALIGN_MENU_LABELS) / sizeof(ALIGN_MENU_LABELS[0]));

static std::vector<std::string> BuildNodeActionMenuItems(bool withAlign)
{
    std::vector<std::string> items = {"Copy", "Delete"};
    if (withAlign) {
        items.push_back("Align");
    }
    return items;
}

// Submenu index of "Auto Layout", appended after the align ops.
static constexpr int ALIGN_AUTO_LAYOUT_INDEX = ALIGN_OP_COUNT;

static std::vector<std::string> BuildAlignSubmenuItems()
{
    std::vector<std::string> items;
    for (int i = 0; i < ALIGN_OP_COUNT; ++i) {
        items.push_back(ALIGN_MENU_LABELS[i]);
    }
    items.push_back("Auto Layout");
    return items;
}

static void ApplyAlign(AlignOp op, const std::vector<NodeId>& selectedNodes, NodeGraph& graph,
                       const NodeLayoutCache& layoutCache, UndoStack& undoStack)
{
    std::vector<NodeMove> moves = ComputeAlignMoves(op, selectedNodes, graph, layoutCache);
    if (!moves.empty()) {
        undoStack.Execute(std::make_unique<MoveNodesCommand>(std::move(moves)), graph);
    }
}

static void ApplyAutoLayout(const std::vector<NodeId>& nodeIds, NodeGraph& graph,
                            const NodeLayoutCache& layoutCache, UndoStack& undoStack)
{
    std::vector<NodeMove> moves = ComputeAutoLayoutMoves(nodeIds, graph, layoutCache);
    if (!moves.empty()) {
        undoStack.Execute(std::make_unique<MoveNodesCommand>(std::move(moves)), graph);
    }
}

static std::vector<NodeId> AllNodeIds(const NodeGraph& graph)
{
    std::vector<NodeId> ids;
    for (const Node& node : graph.GetNodes()) {
        ids.push_back(node.id);
    }
    return ids;
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

    if (action.fieldIndex >= 0) {
        // Struct field edit: parse the text as the field's own type.
        const UserType* structType = UserTypeRegistry::Find(def.typeName);
        if (structType == nullptr
            || action.fieldIndex >= static_cast<int>(structType->fields.size())) {
            return;
        }
        const StructField& field =
            structType->fields[static_cast<std::size_t>(action.fieldIndex)];
        PropertyValue fieldValue;
        if (!ParseValueString(action.text, field.type, fieldValue.scalar)) {
            std::printf("property '%s.%s': invalid value\n", def.name.c_str(),
                        field.name.c_str());
            return;
        }
        undoStack.Execute(
            std::make_unique<SetNodePropertyCommand>(node.id, action.propertyIndex,
                                                     std::move(fieldValue), action.fieldIndex),
            graph);
        return;
    }

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
                               CanvasController& controller, TabRenameState& tabRename,
                               std::vector<std::string>& logLines)
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
    case TabBarHit::Kind::Run:
        RunActiveGraph(*documents[static_cast<std::size_t>(activeDocIndex)], logLines);
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

    MoveConsoleToLeftMonitorMaximized();

    LoadCustomNodeClasses();
    LoadWasmModules();
    {
        std::error_code ec;
        std::filesystem::create_directories(WASM_SOURCE_DIR, ec);
        std::string headerError;
        if (!WriteWasmApiHeader(std::string(WASM_SOURCE_DIR) + "/gau_api.h", headerError)) {
            std::printf("gau_api.h: %s\n", headerError.c_str());
        }
    }

    EditorSettings settings;
    settings.LoadFromFile(EDITOR_SETTINGS_PATH);

    PlatformWindow window;
    if (!window.Init("GAU Node Editor", settings.windowWidth, settings.windowHeight)) {
        return 1;
    }
    if (settings.windowMaximized) {
        window.Maximize();
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
    FunctionEditorDialog functionEditor;
    functionEditor.SetCharWidth(MeasureMonoCharWidth(vg, 12.0f * UI_SCALE));
    PropertyPanel propertyPanel;
    ActionMenu actionMenu;
    NodeId actionTargetNodeId = INVALID_ID;
    CommentId actionTargetCommentId = INVALID_ID;
    GraphClipboard clipboard;
    TabRenameState tabRename;
    LogPanel logPanel;
    std::vector<std::string> logLines;
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

            if (functionEditor.IsOpen()) {
                // Clipboard keys bridge to the OS clipboard here (the
                // dialog itself owns only caret/selection state).
                if (event.type == EditorInputType::KeyDown) {
                    if (event.key == EditorKey::Copy || event.key == EditorKey::Cut) {
                        const std::string selected = functionEditor.GetSelectedText();
                        if (!selected.empty()) {
                            SetClipboardText(selected);
                            if (event.key == EditorKey::Cut) {
                                functionEditor.DeleteSelectedText();
                            }
                        }
                        continue;
                    }
                    if (event.key == EditorKey::Paste) {
                        functionEditor.InsertAtCaret(GetClipboardText());
                        continue;
                    }
                }
                const FunctionEditorAction action = functionEditor.HandleEvent(event);
                if (action.type == FunctionEditorAction::Type::Build) {
                    BuildWasmFunction(action, settings, functionEditor, documents, logLines);
                }
                continue;
            }

            if (classDialog.IsOpen()) {
                const ClassEditorAction action = classDialog.HandleEvent(event);
                ProcessClassEditorAction(action, doc.graph);
                continue;
            }

            if (actionMenu.IsOpen()) {
                const ActionMenuResult result = actionMenu.HandleEvent(event);
                if (result.type == ActionMenuResult::Type::Selected) {
                    if (actionTargetNodeId != INVALID_ID) {
                        std::vector<NodeId> targetIds;
                        if (controller.IsSelected(actionTargetNodeId)) {
                            targetIds = controller.selectedNodes;
                        } else {
                            targetIds.push_back(actionTargetNodeId);
                        }
                        if (result.submenuParent >= 0) {
                            if (result.submenuIndex >= 0 && result.submenuIndex < ALIGN_OP_COUNT) {
                                ApplyAlign(static_cast<AlignOp>(result.submenuIndex),
                                           controller.selectedNodes, doc.graph, layoutCache,
                                           doc.undoStack);
                            } else if (result.submenuIndex == ALIGN_AUTO_LAYOUT_INDEX) {
                                ApplyAutoLayout(controller.selectedNodes, doc.graph, layoutCache,
                                                doc.undoStack);
                            }
                        } else if (result.itemIndex == 0) {
                            clipboard = CopyNodesToClipboard(doc.graph, targetIds);
                        } else if (result.itemIndex == 1) {
                            doc.undoStack.Execute(
                                std::make_unique<DeleteNodesCommand>(std::move(targetIds)),
                                doc.graph);
                            controller.selectedNodes.clear();
                        }
                    } else if (actionTargetCommentId != INVALID_ID && result.itemIndex == 0) {
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
                if (action.type == ContextMenuAction::Type::Paste) {
                    PasteClipboardAt(clipboard, contextMenu.GetSpawnCanvasX(),
                                     contextMenu.GetSpawnCanvasY(), doc.graph, doc.undoStack,
                                     controller.selectedNodes);
                } else if (action.type == ContextMenuAction::Type::OpenFunctionEditor) {
                    functionEditor.Open(screenWidth, screenHeight);
                } else if (action.type == ContextMenuAction::Type::ArrangeNodes) {
                    ApplyAutoLayout(AllNodeIds(doc.graph), doc.graph, layoutCache, doc.undoStack);
                } else {
                    ProcessMenuAction(action, contextMenu, doc.graph, doc.undoStack,
                                      classDialog, screenWidth, screenHeight,
                                      layoutCache, controller.selectedNodes);
                }
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
                                       window, controller, tabRename, logLines);
                    // Blocking dialogs may have re-pumped the event list;
                    // stop iterating this frame's events.
                    break;
                }
                continue;
            }

            // Bottom log panel swallows clicks/wheel over it.
            {
                bool clearLog = false;
                if (logPanel.HandleEvent(event, static_cast<int>(logLines.size()),
                                         screenWidth, screenHeight, clearLog)) {
                    if (clearLog) {
                        logLines.clear();
                    }
                    continue;
                }
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
                    contextMenu.SetPasteAvailable(!clipboard.IsEmpty());
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
                if (event.key == EditorKey::Copy) {
                    if (!controller.selectedNodes.empty()) {
                        clipboard = CopyNodesToClipboard(doc.graph, controller.selectedNodes);
                    }
                    continue;
                }
                if (event.key == EditorKey::Paste) {
                    const Vec2 canvasPos = doc.canvas.ScreenToCanvas(
                        Vec2{controller.lastMouseX, controller.lastMouseY});
                    PasteClipboardAt(clipboard, canvasPos.x, canvasPos.y,
                                     doc.graph, doc.undoStack, controller.selectedNodes);
                    continue;
                }
                if (event.key == EditorKey::StraightenConnections) {
                    ApplyAlign(AlignOp::Straighten, controller.selectedNodes, doc.graph,
                               layoutCache, doc.undoStack);
                    continue;
                }
            }

            // Double-clicking a node opens its class in the editor.
            if (event.type == EditorInputType::MouseDown
                && event.button == EditorMouseButton::Left
                && event.clicks >= 2 && !event.alt && !event.ctrl) {
                const Vec2 canvasPos = doc.canvas.ScreenToCanvas(Vec2{event.x, event.y});
                const NodeId hitNodeId = HitTestNode(layoutCache, canvasPos.x, canvasPos.y);
                if (hitNodeId != INVALID_ID) {
                    const Node* node = doc.graph.FindNode(hitNodeId);
                    if (node != nullptr) {
                        if (node->nodeClass->IsDynamic()) {
                            classDialog.OpenForEdit(*node->nodeClass, screenWidth, screenHeight);
                        } else {
                            std::printf("builtin class cannot be edited: %s\n",
                                        node->nodeClass->GetName());
                        }
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
                    const bool multiSelected =
                        controller.IsSelected(hitNodeId) && controller.selectedNodes.size() >= 2;
                    std::vector<int> separators;
                    if (multiSelected) {
                        // Divider between the base actions and "Align".
                        separators = {NODE_MENU_BASE_ITEMS - 1};
                    }
                    actionMenu.Open(event.x, event.y, BuildNodeActionMenuItems(multiSelected),
                                    screenWidth, screenHeight, std::move(separators));
                    if (multiSelected) {
                        // Submenu dividers: X align | Y align | distribute |
                        // straighten | auto layout (AlignOp order).
                        actionMenu.SetSubmenu(NODE_MENU_BASE_ITEMS, BuildAlignSubmenuItems(),
                                              {2, 5, 7, 8});
                    }
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
                contextMenu.SetPasteAvailable(!clipboard.IsEmpty());
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
        if (doc.previewRevision != doc.undoStack.GetRevision()) {
            ExecEngine previewEngine(doc.graph, [](const std::string&) {});
            doc.previewValues = previewEngine.EvaluateDataPreview();
            doc.previewRevision = doc.undoStack.GetRevision();
        }
        RenderNodes(vg, doc.canvas, doc.graph, layoutCache, controller.selectedNodes,
                    controller.draggingLinkPinId,
                    controller.linkDragCanvasX, controller.linkDragCanvasY,
                    doc.previewValues);
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

        DrawLogPanel(vg, logPanel, logLines, screenWidth, screenHeight);
        DrawContextMenu(vg, contextMenu);
        DrawActionMenu(vg, actionMenu);
        DrawClassEditorDialog(vg, classDialog, screenWidth, screenHeight);
        DrawFunctionEditorDialog(vg, functionEditor, screenWidth, screenHeight);
        nvgEndFrame(vg);

        window.EndFrame();
    }

    settings.collapsedCategories = contextMenu.GetCollapsedCategories();
    CollectSessionDocuments(settings, documents, activeDocIndex);
    settings.windowMaximized = window.IsMaximized();
    if (!settings.windowMaximized) {
        // Keep the last windowed size; a maximized exit must not
        // overwrite it with the maximized dimensions.
        settings.windowWidth = window.GetWidth();
        settings.windowHeight = window.GetHeight();
    }
    if (!settings.SaveToFile(EDITOR_SETTINGS_PATH)) {
        std::printf("editor_settings.json: failed to save settings\n");
    }

    DestroyPlatformNVGContext(vg);
    window.Shutdown();
    return 0;
}
