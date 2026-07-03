#include "platform/PlatformWindow.h"
#include "platform/PlatformNVG.h"
#include "platform/PlatformFileDialog.h"
#include "render/Canvas.h"
#include "render/GridRenderer.h"
#include "render/SelectionRenderer.h"
#include "render/CommentRenderer.h"
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
#include "interaction/HitTest.h"
#include "interaction/TabBar.h"
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
                const NodeId hitNodeId = HitTestNode(layoutCache, canvasPos.x, canvasPos.y);
                lastMouseX = event.x;
                lastMouseY = event.y;
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
                        const std::vector<NodeId>& selectedNodes)
{
    layoutCache.Clear();

    nvgSave(vg);
    nvgScale(vg, canvas.GetZoom(), canvas.GetZoom());
    nvgTranslate(vg, -canvas.GetPanX(), -canvas.GetPanY());

    for (const Node& node : graph.GetNodes()) {
        NodeLayout layout = ComputeNodeLayout(vg, node);
        DrawNode(vg, node, layout, ContainsNodeId(selectedNodes, node.id));
        layoutCache.Add(layout);
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

// Asks about unsaved changes when needed. Returns false when closing
// must be aborted (cancel or failed/cancelled save).
static bool ConfirmDocumentClose(Document& doc, PlatformWindow& window)
{
    if (!doc.IsDirty()) {
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

// Prompts for every dirty document before quitting; documents stay open
// so the session file still records them. Returns false to abort quit.
static bool ConfirmQuit(std::vector<std::unique_ptr<Document>>& documents,
                        PlatformWindow& window)
{
    for (std::unique_ptr<Document>& doc : documents) {
        if (!ConfirmDocumentClose(*doc, window)) {
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
    for (const OpenFileEntry& openFile : settings.openFiles) {
        std::error_code ec;
        if (!std::filesystem::exists(openFile.path, ec)) {
            std::printf("session restore: file missing, skipped: %s\n", openFile.path.c_str());
            continue;
        }
        if (OpenDocumentFromPath(openFile.path, documents, window) && openFile.zoom > 0.0f) {
            documents.back()->canvas.SetView(openFile.panX, openFile.panY, openFile.zoom);
        }
    }

    for (int i = 0; i < static_cast<int>(documents.size()); ++i) {
        if (!settings.activeFilePath.empty()
            && documents[static_cast<std::size_t>(i)]->filePath == settings.activeFilePath) {
            return i;
        }
    }
    return 0;
}

// Records the open documents (files only) for the next session.
static void CollectSessionDocuments(EditorSettings& settings,
                                    const std::vector<std::unique_ptr<Document>>& documents,
                                    int activeDocIndex)
{
    settings.openFiles.clear();
    for (const std::unique_ptr<Document>& doc : documents) {
        if (doc->filePath.empty()) {
            continue;
        }
        OpenFileEntry entry;
        entry.path = doc->filePath;
        entry.panX = doc->canvas.GetPanX();
        entry.panY = doc->canvas.GetPanY();
        entry.zoom = doc->canvas.GetZoom();
        settings.openFiles.push_back(std::move(entry));
    }
    settings.activeFilePath = documents[static_cast<std::size_t>(activeDocIndex)]->filePath;
}

// Handles a left click on the tab bar. Returns true when consumed.
static bool ProcessTabBarClick(const EditorInputEvent& event, float screenWidth,
                               std::vector<std::unique_ptr<Document>>& documents,
                               int& activeDocIndex, PlatformWindow& window,
                               CanvasController& controller)
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
        }
        break;
    case TabBarHit::Kind::CloseTab:
        if (!ConfirmDocumentClose(*documents[static_cast<std::size_t>(hit.tabIndex)], window)) {
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

            if (contextMenu.IsOpen()) {
                const ContextMenuAction action = contextMenu.HandleEvent(event);
                ProcessMenuAction(action, contextMenu, doc.graph, doc.undoStack,
                                  classDialog, screenWidth, screenHeight,
                                  layoutCache, controller.selectedNodes);
                continue;
            }

            // Inline comment renaming consumes keyboard input first.
            if (controller.IsEditingTitle()
                && (event.type == EditorInputType::KeyDown
                    || event.type == EditorInputType::TextInput)) {
                controller.HandleEvent(event, doc.canvas, doc.graph, layoutCache, doc.undoStack);
                continue;
            }

            // Clicks in the tab bar never reach the canvas.
            if (event.type == EditorInputType::MouseDown && event.y < TAB_BAR_HEIGHT) {
                if (event.button == EditorMouseButton::Left) {
                    ProcessTabBarClick(event, screenWidth, documents, activeDocIndex,
                                       window, controller);
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
            }

            if (controller.HandleEvent(event, doc.canvas, doc.graph, layoutCache, doc.undoStack)) {
                const Vec2 canvasPos = doc.canvas.ScreenToCanvas(Vec2{event.x, event.y});
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
        RenderNodes(vg, doc.canvas, doc.graph, layoutCache, controller.selectedNodes);
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
        DrawTabBar(vg, tabNames, activeDocIndex, screenWidth);

        DrawContextMenu(vg, contextMenu);
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
