#include "platform/PlatformWindow.h"
#include "platform/PlatformNVG.h"
#include "render/Canvas.h"
#include "render/GridRenderer.h"
#include "render/NodeRenderer.h"
#include "render/NodeLayoutCache.h"
#include "render/ContextMenuRenderer.h"
#include "model/NodeGraph.h"
#include "model/NodeClassLoader.h"
#include "model/EditorSettings.h"
#include "model/UndoStack.h"
#include "model/GraphCommands.h"
#include "interaction/EditorInputEvent.h"
#include "interaction/ContextMenu.h"
#include "interaction/ClassEditorDialog.h"
#include "render/ClassEditorDialogRenderer.h"

#include <nanovg.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>

// Canvas background color from the design spec: rgb(30,30,33).
static const float BACKGROUND_R = 30.0f / 255.0f;
static const float BACKGROUND_G = 30.0f / 255.0f;
static const float BACKGROUND_B = 33.0f / 255.0f;

// Zoom multiplier per wheel notch.
static const float ZOOM_STEP = 1.1f;

// Right-button release with less accumulated motion than this is a click
// (opens the context menu); more is a pan drag.
static const float CLICK_DRAG_THRESHOLD = 4.0f;

// Temporary M1/M2 input wiring: right-drag pan, wheel zoom, right-click
// detection for the context menu. Replaced by InteractionFSM in M3.
struct CanvasController
{
    bool panning = false;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    float dragDistance = 0.0f;

    // Returns true when a right click (press+release without dragging)
    // completed; the caller opens the context menu at the event position.
    bool HandleEvent(const EditorInputEvent& event, Canvas& canvas)
    {
        switch (event.type) {
        case EditorInputType::MouseDown:
            if (event.button == EditorMouseButton::Right) {
                panning = true;
                dragDistance = 0.0f;
                lastMouseX = event.x;
                lastMouseY = event.y;
            }
            break;

        case EditorInputType::MouseUp:
            if (event.button == EditorMouseButton::Right && panning) {
                panning = false;
                if (dragDistance < CLICK_DRAG_THRESHOLD) {
                    return true;
                }
            }
            break;

        case EditorInputType::MouseMove:
            if (panning) {
                const float dx = event.x - lastMouseX;
                const float dy = event.y - lastMouseY;
                canvas.PanByScreenDelta(dx, dy);
                dragDistance += std::fabs(dx) + std::fabs(dy);
            }
            lastMouseX = event.x;
            lastMouseY = event.y;
            break;

        case EditorInputType::MouseWheel:
            canvas.ZoomAt(Vec2{event.x, event.y}, std::pow(ZOOM_STEP, event.wheelDelta));
            break;

        case EditorInputType::KeyDown:
        case EditorInputType::TextInput:
            break;
        }
        return false;
    }
};

static void ProcessMenuAction(const ContextMenuAction& action, const ContextMenu& menu,
                              NodeGraph& graph, UndoStack& undoStack,
                              ClassEditorDialog& classDialog, float screenWidth, float screenHeight)
{
    if (action.type == ContextMenuAction::Type::OpenClassEditor) {
        classDialog.Open(screenWidth, screenHeight);
        return;
    }
    if (action.type != ContextMenuAction::Type::CreateNode || action.nodeClass == nullptr) {
        return;
    }
    undoStack.Execute(
        std::make_unique<AddNodeCommand>(*action.nodeClass, menu.GetSpawnCanvasX(), menu.GetSpawnCanvasY()),
        graph);
}

// Registers the class from a validated dialog submission and persists it
// to custom_nodes.json so it survives restarts.
static void ProcessClassEditorAction(const ClassEditorAction& action)
{
    if (action.type != ClassEditorAction::Type::Submit) {
        return;
    }

    NodeClass::AdoptDynamic(
        std::make_unique<NodeClass>(action.name, action.category, action.pins));

    std::string error;
    if (!AppendNodeClassToFile("custom_nodes.json", action.name, action.category,
                               action.pins, error)) {
        std::printf("custom_nodes.json: %s\n", error.c_str());
    }
}

static void RenderNodes(NVGcontext* vg, const Canvas& canvas,
                        const NodeGraph& graph, NodeLayoutCache& layoutCache)
{
    layoutCache.Clear();

    nvgSave(vg);
    nvgScale(vg, canvas.GetZoom(), canvas.GetZoom());
    nvgTranslate(vg, -canvas.GetPanX(), -canvas.GetPanY());

    for (const Node& node : graph.GetNodes()) {
        NodeLayout layout = ComputeNodeLayout(vg, node);
        DrawNode(vg, node, layout, false);
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

    Canvas canvas;
    CanvasController controller;
    NodeGraph graph;
    UndoStack undoStack;
    NodeLayoutCache layoutCache;
    ContextMenu contextMenu;
    ClassEditorDialog classDialog;
    std::vector<EditorInputEvent> events;

    for (NodeCategory category : ALL_NODE_CATEGORIES) {
        contextMenu.SetCategoryCollapsed(category, settings.categoryCollapsed[NodeCategoryIndex(category)]);
    }

    while (window.PumpEvents(events)) {
        const float screenWidth = static_cast<float>(window.GetWidth());
        const float screenHeight = static_cast<float>(window.GetHeight());

        for (const EditorInputEvent& event : events) {
            if (classDialog.IsOpen()) {
                const ClassEditorAction action = classDialog.HandleEvent(event);
                ProcessClassEditorAction(action);
                continue;
            }

            if (contextMenu.IsOpen()) {
                const ContextMenuAction action = contextMenu.HandleEvent(event);
                ProcessMenuAction(action, contextMenu, graph, undoStack,
                                  classDialog, screenWidth, screenHeight);
                continue;
            }

            if (event.type == EditorInputType::KeyDown) {
                if (event.key == EditorKey::Undo) {
                    undoStack.Undo(graph);
                    continue;
                }
                if (event.key == EditorKey::Redo) {
                    undoStack.Redo(graph);
                    continue;
                }
                if (event.key == EditorKey::Tab) {
                    const float mouseX = controller.lastMouseX;
                    const float mouseY = controller.lastMouseY;
                    const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{mouseX, mouseY});
                    contextMenu.Open(mouseX, mouseY, canvasPos.x, canvasPos.y,
                                     screenWidth, screenHeight);
                    continue;
                }
            }

            if (controller.HandleEvent(event, canvas)) {
                const Vec2 canvasPos = canvas.ScreenToCanvas(Vec2{event.x, event.y});
                contextMenu.Open(event.x, event.y, canvasPos.x, canvasPos.y,
                                 screenWidth, screenHeight);
            }
        }

        window.BeginFrame(BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);

        nvgBeginFrame(vg, screenWidth, screenHeight, window.GetPixelRatio());
        DrawGrid(vg, canvas, screenWidth, screenHeight);
        RenderNodes(vg, canvas, graph, layoutCache);
        DrawContextMenu(vg, contextMenu);
        DrawClassEditorDialog(vg, classDialog, screenWidth, screenHeight);
        nvgEndFrame(vg);

        window.EndFrame();
    }

    for (NodeCategory category : ALL_NODE_CATEGORIES) {
        settings.categoryCollapsed[NodeCategoryIndex(category)] = contextMenu.IsCategoryCollapsed(category);
    }
    if (!settings.SaveToFile(EDITOR_SETTINGS_PATH)) {
        std::printf("editor_settings.json: failed to save settings\n");
    }

    DestroyPlatformNVGContext(vg);
    window.Shutdown();
    return 0;
}
