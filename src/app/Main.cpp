// v2 application entry: wires core/model/exec/ui/render on the v1 platform
// into a minimal runnable editor -- a demo graph the user can pan/zoom and
// a Run button that executes the graph through the v2 runtime. Assembly
// grows into Application/Document/InputRouter in later slices.

#include "platform/PlatformFileDialog.h"
#include "platform/PlatformNVG.h"
#include "platform/PlatformWindow.h"

#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/ConversionNodes.h"
#include "exec/FunctionInterface.h"
#include "exec/FunctionNodes.h"
#include "exec/FunctionOps.h"
#include "exec/Runtime.h"
#include "exec/StructEdit.h"
#include "exec/StructNodes.h"
#include "exec/VariableNodes.h"
#include "exec/WasmHost.h"
#include "exec/WasmNodes.h"
#include "interaction/Align.h"
#include "interaction/HitTest2.h"
#include "interaction/InteractionFsm.h"
#include "interaction/Minimap.h"
#include "interaction/NodeSearch.h"
#include "io/ProjectExport.h"
#include "io/ProjectFile.h"
#include "model/Function.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "model/Project.h"
#include "model/UndoHistory.h"
#include "render/GraphLayout.h"
#include "render/GraphRenderer.h"
#include "render/NanoVgPainter.h"
#include "render/RenderCanvas.h"
#include "ui/Widget.h"

#include <nanovg.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace gau;

// Editor session state persisted across runs: window shape, view
// transform, panel toggles and the last opened project (reopened on
// start). Lives next to the executable / working directory.
static const char* SESSION_FILE = "gau_session.json";

struct SessionState
{
    int windowW = 1280;
    int windowH = 800;
    bool maximized = false;
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 1.0f;
    std::string projectPath;
    bool showVars = false;
    bool showTypes = false;
    bool showWasm = false;
};

static SessionState LoadSession()
{
    SessionState state;
    std::ifstream file(SESSION_FILE, std::ios::binary);
    if (!file.is_open()) {
        return state;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const nlohmann::json root = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return state;
    }
    state.windowW = root.value("windowW", state.windowW);
    state.windowH = root.value("windowH", state.windowH);
    state.maximized = root.value("maximized", state.maximized);
    state.panX = root.value("panX", state.panX);
    state.panY = root.value("panY", state.panY);
    state.zoom = root.value("zoom", state.zoom);
    state.projectPath = root.value("projectPath", state.projectPath);
    state.showVars = root.value("showVars", state.showVars);
    state.showTypes = root.value("showTypes", state.showTypes);
    state.showWasm = root.value("showWasm", state.showWasm);
    if (state.windowW < 320) {
        state.windowW = 1280;
    }
    if (state.windowH < 240) {
        state.windowH = 800;
    }
    return state;
}

static void SaveSession(const SessionState& state)
{
    nlohmann::json root;
    root["windowW"] = state.windowW;
    root["windowH"] = state.windowH;
    root["maximized"] = state.maximized;
    root["panX"] = state.panX;
    root["panY"] = state.panY;
    root["zoom"] = state.zoom;
    root["projectPath"] = state.projectPath;
    root["showVars"] = state.showVars;
    root["showTypes"] = state.showTypes;
    root["showWasm"] = state.showWasm;
    std::ofstream file(SESSION_FILE, std::ios::binary);
    if (file.is_open()) {
        file << root.dump(2) << "\n";
    }
}

static NodeClass Cls(std::string name, std::string category, std::vector<PinDef> pins,
                     std::vector<PropertyDef> props = {})
{
    NodeClass c;
    c.name = std::move(name);
    c.category = std::move(category);
    c.pins = std::move(pins);
    c.properties = std::move(props);
    return c;
}

static void RegisterDemoClasses(NodeClassRegistry& classes, const TypeRegistry& t)
{
    const TypeId exec = t.Builtin(TypeTag::Exec);
    const TypeId i = t.Builtin(TypeTag::Int);
    classes.Register(Cls("EventBegin", "Event", {{PinDirection::Output, exec, "Then"}}));
    classes.Register(Cls("MakeInt", "Pure", {{PinDirection::Output, i, "Value"}},
                         {{"Value", i, Value::Int(42)}}));
    classes.Register(Cls("PrintInt", "Function",
                         {{PinDirection::Input, exec, "Exec"},
                          {PinDirection::Input, i, "Value"},
                          {PinDirection::Output, exec, "Then"}}));
}

static NodeId FindByClass(const Graph& graph, const std::string& className)
{
    for (const Node& n : graph.Nodes()) {
        if (n.className == className) {
            return n.id;
        }
    }
    return INVALID_ID;
}

static NodeId BuildDemoGraph(Graph& graph, const NodeClassRegistry& classes)
{
    const NodeId ev = graph.AddNode(*classes.Find("EventBegin"), 40.0f, 40.0f);
    const NodeId mi = graph.AddNode(*classes.Find("MakeInt"), 40.0f, 180.0f);
    const NodeId print = graph.AddNode(*classes.Find("PrintInt"), 320.0f, 60.0f);
    graph.AddLink(graph.FindNode(ev)->outputs[0].id, graph.FindNode(print)->inputs[0].id);
    graph.AddLink(graph.FindNode(mi)->outputs[0].id, graph.FindNode(print)->inputs[1].id);
    return ev;
}

static ui::Event Translate(const EditorInputEvent& e)
{
    ui::Event u;
    u.x = e.x;
    u.y = e.y;
    switch (e.type) {
    case EditorInputType::MouseMove:
        u.type = ui::EventType::MouseMove;
        break;
    case EditorInputType::MouseDown:
        u.type = ui::EventType::MouseDown;
        break;
    case EditorInputType::MouseUp:
        u.type = ui::EventType::MouseUp;
        break;
    case EditorInputType::MouseWheel:
        u.type = ui::EventType::Wheel;
        u.wheel = e.wheelDelta;
        break;
    case EditorInputType::KeyDown:
        u.type = ui::EventType::Key;
        if (e.key == EditorKey::Backspace) {
            u.key = ui::keys::Backspace;
        } else if (e.key == EditorKey::Enter) {
            u.key = ui::keys::Enter;
        } else if (e.key == EditorKey::Escape) {
            u.key = ui::keys::Escape;
        }
        break;
    case EditorInputType::TextInput:
        u.type = ui::EventType::Text;
        std::memcpy(u.text, e.text, sizeof(u.text));
        break;
    }
    if (e.button == EditorMouseButton::Left) {
        u.button = 0;
    } else if (e.button == EditorMouseButton::Right) {
        u.button = 1;
    } else {
        u.button = 2;
    }
    return u;
}

// Parses an edited property string back into a scalar Value of the pin type.
static Value ParseValue(const TypeRegistry& types, TypeId id, const std::string& s)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        return Value::None();
    }
    switch (desc->tag) {
    case TypeTag::Bool:
        return Value::Bool(s == "true" || s == "1");
    case TypeTag::Int:
    case TypeTag::Enum:
        return Value::Int(std::strtoll(s.c_str(), nullptr, 10));
    case TypeTag::Float:
        return Value::Float(std::strtod(s.c_str(), nullptr));
    case TypeTag::String:
        return Value::Str(s);
    default:
        return types.MakeDefault(id);
    }
}

// Draws comment/group boxes behind the graph (FR-UX-4).
static void DrawComments(NVGcontext* vg, const render::Canvas& canvas,
                         const std::vector<Comment>& comments)
{
    for (const Comment& c : comments) {
        const render::Vec2 tl = canvas.CanvasToScreen(render::Vec2{c.x, c.y});
        const float w = c.w * canvas.Zoom();
        const float h = c.h * canvas.Zoom();
        nvgBeginPath(vg);
        nvgRoundedRect(vg, tl.x, tl.y, w, h, 4.0f);
        nvgFillColor(vg, nvgRGBA(90, 90, 130, 40));
        nvgFill(vg);
        nvgStrokeColor(vg, nvgRGBA(140, 140, 180, 180));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
        nvgFillColor(vg, nvgRGBA(210, 210, 230, 220));
        nvgFontSize(vg, 14.0f);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(vg, tl.x + 6.0f, tl.y + 4.0f, c.text.c_str(), nullptr);
    }
}

// Two-level background grid (design spec section 5): 16px minor cells
// and 8x major cells in canvas units, panning and zooming with the
// graph. Minor lines fade out when they would pack tighter than a few
// pixels.
static void DrawGrid(NVGcontext* vg, const render::Canvas& canvas, float screenW, float screenH)
{
    const auto drawLines = [&](float canvasStep, NVGcolor color) {
        const float step = canvasStep * canvas.Zoom();
        if (step < 4.0f) {
            return;
        }
        const render::Vec2 pan = canvas.Pan();
        float x = std::fmod(pan.x, step);
        if (x < 0.0f) {
            x += step;
        }
        nvgBeginPath(vg);
        for (; x < screenW; x += step) {
            nvgMoveTo(vg, x, 0.0f);
            nvgLineTo(vg, x, screenH);
        }
        float y = std::fmod(pan.y, step);
        if (y < 0.0f) {
            y += step;
        }
        for (; y < screenH; y += step) {
            nvgMoveTo(vg, 0.0f, y);
            nvgLineTo(vg, screenW, y);
        }
        nvgStrokeColor(vg, color);
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    };
    drawLines(16.0f, nvgRGB(41, 41, 43));
    drawLines(128.0f, nvgRGB(26, 26, 28));
}

// A lightweight popup menu drawn with NanoVG (v1-style right-click UX):
// category headers plus clickable entries, hover highlight, wheel scroll.
struct PopupMenu
{
    struct Item
    {
        std::string label;
        bool header = false;
        // Class name for spawn entries; action id for action entries.
        std::string tag;
    };

    bool open = false;
    float x = 0.0f;
    float y = 0.0f;
    // Canvas-space spawn location captured when the menu opened.
    float canvasX = 0.0f;
    float canvasY = 0.0f;
    int hover = -1;
    float scroll = 0.0f;
    std::vector<Item> items;

    static constexpr float WIDTH = 250.0f;
    static constexpr float ITEM_H = 27.0f;
    static constexpr float MAX_H = 460.0f;
    static constexpr float PAD = 4.0f;

    float ContentHeight() const
    {
        return static_cast<float>(items.size()) * ITEM_H + PAD * 2.0f;
    }

    float Height() const
    {
        const float h = ContentHeight();
        return h < MAX_H ? h : MAX_H;
    }

    bool Contains(float px, float py) const
    {
        return px >= x && px <= x + WIDTH && py >= y && py <= y + Height();
    }

    int IndexAt(float px, float py) const
    {
        if (!Contains(px, py)) {
            return -1;
        }
        const int index = static_cast<int>((py - y - PAD + scroll) / ITEM_H);
        return (index >= 0 && index < static_cast<int>(items.size())) ? index : -1;
    }

    void ScrollBy(float delta)
    {
        scroll -= delta * ITEM_H;
        const float maxScroll = ContentHeight() - Height();
        if (scroll < 0.0f) {
            scroll = 0.0f;
        }
        if (scroll > maxScroll) {
            scroll = maxScroll > 0.0f ? maxScroll : 0.0f;
        }
    }
};

static void DrawPopupMenu(NVGcontext* vg, const PopupMenu& menu)
{
    if (!menu.open) {
        return;
    }
    const float h = menu.Height();

    nvgBeginPath(vg);
    nvgRoundedRect(vg, menu.x, menu.y, PopupMenu::WIDTH, h, 4.0f);
    nvgFillColor(vg, nvgRGBA(28, 28, 32, 248));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(70, 70, 78, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgSave(vg);
    nvgIntersectScissor(vg, menu.x, menu.y, PopupMenu::WIDTH, h);
    nvgFontSize(vg, 15.0f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        const PopupMenu::Item& item = menu.items[i];
        const float rowY = menu.y + PopupMenu::PAD + static_cast<float>(i) * PopupMenu::ITEM_H
                         - menu.scroll;
        if (rowY + PopupMenu::ITEM_H < menu.y || rowY > menu.y + h) {
            continue;
        }
        if (!item.header && static_cast<int>(i) == menu.hover) {
            nvgBeginPath(vg);
            nvgRect(vg, menu.x + 3.0f, rowY, PopupMenu::WIDTH - 6.0f, PopupMenu::ITEM_H);
            nvgFillColor(vg, nvgRGBA(50, 90, 160, 255));
            nvgFill(vg);
        }
        if (item.header) {
            nvgFontFace(vg, "sans-bold");
            nvgFillColor(vg, nvgRGBA(150, 150, 160, 255));
            nvgText(vg, menu.x + 10.0f, rowY + PopupMenu::ITEM_H * 0.5f, item.label.c_str(),
                    nullptr);
        } else {
            nvgFontFace(vg, "sans");
            nvgFillColor(vg, nvgRGBA(230, 230, 235, 255));
            nvgText(vg, menu.x + 22.0f, rowY + PopupMenu::ITEM_H * 0.5f, item.label.c_str(),
                    nullptr);
        }
    }
    nvgRestore(vg);
}

// Minimap panel rect, anchored to the bottom-right corner.
static ViewRect MinimapPanelRect(float screenW, float screenH)
{
    return ViewRect{screenW - 240.0f - 16.0f, screenH - 130.0f - 16.0f, 240.0f, 130.0f};
}

// Draws a minimap of all nodes plus the current viewport (FR-UX-1).
static void DrawMinimap(NVGcontext* vg, const render::Canvas& canvas,
                        const render::GraphLayout& layout, float screenW, float screenH)
{
    std::vector<NodeBox> boxes;
    for (const render::NodeLayout& nl : layout.Nodes()) {
        boxes.push_back(NodeBox{nl.id, nl.x, nl.y, nl.w, nl.h});
    }
    Bounds content;
    if (!ComputeBounds(boxes, content)) {
        return;
    }
    const ViewRect panel = MinimapPanelRect(screenW, screenH);
    const render::Vec2 tl = canvas.ScreenToCanvas(render::Vec2{0.0f, 0.0f});
    const render::Vec2 br = canvas.ScreenToCanvas(render::Vec2{screenW, screenH});
    const Bounds visible{tl.x, tl.y, br.x, br.y};
    const MinimapFit fit = ComputeMinimap(content, panel, visible);

    nvgBeginPath(vg);
    nvgRect(vg, panel.x, panel.y, panel.w, panel.h);
    nvgFillColor(vg, nvgRGBA(20, 20, 24, 220));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(70, 70, 78, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Everything inside the minimap clips to the panel: the viewport
    // rectangle regularly extends past the content bounds when zoomed
    // out and must not spill over the rest of the UI.
    nvgSave(vg);
    nvgIntersectScissor(vg, panel.x, panel.y, panel.w, panel.h);

    for (const NodeBox& b : boxes) {
        nvgBeginPath(vg);
        nvgRect(vg, fit.offsetX + b.x * fit.scale, fit.offsetY + b.y * fit.scale,
                std::max(b.w * fit.scale, 1.5f), std::max(b.h * fit.scale, 1.5f));
        nvgFillColor(vg, nvgRGBA(120, 140, 180, 255));
        nvgFill(vg);
    }

    nvgBeginPath(vg);
    nvgRect(vg, fit.viewport.x, fit.viewport.y, fit.viewport.w, fit.viewport.h);
    nvgStrokeColor(vg, nvgRGBA(255, 180, 40, 255));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);

    nvgRestore(vg);
}

int main()
{
    SessionState session = LoadSession();

    PlatformWindow window;
    if (!window.Init("GAU", session.windowW, session.windowH)) {
        return 1;
    }
    if (session.maximized) {
        window.Maximize();
    }
    NVGcontext* vg = CreatePlatformNVGContext();
    if (vg == nullptr) {
        window.Shutdown();
        return 1;
    }
    render::NanoVgPainter painter(vg, "sans");

    // The editable project (types, classes, functions, variables, comments,
    // graph) is one save/load unit; references keep the rest of the code
    // terse. Runtime behaviors live in a separate builtins registry.
    Project project;
    TypeRegistry& types = project.types;
    NodeClassRegistry& classes = project.classes;
    FunctionRegistry& functions = project.functions;
    // The graph currently shown/edited: the main graph, or a function body
    // when editing a function (function edit UI). Run/Debug always target the
    // main graph. Panel callbacks capture `active` (not a fixed reference) so
    // switching context takes effect immediately.
    Graph* active = project.graph.get();
    // The function whose body is being edited (null when editing the main
    // graph); its interface can be grown with Add In / Add Out.
    FunctionDef* editingDef = nullptr;
    // Type chosen for new parameters/variables. The Type button cycles
    // through the builtin scalars plus every user-defined type currently
    // registered (struct/enum/object), per SRS 2.1 / FR-TYP-1: user types
    // must be selectable on pins and properties.
    TypeId selTypeId = types.Builtin(TypeTag::Int);
    const auto selectableTypes = [&]() -> std::vector<TypeId> {
        std::vector<TypeId> list{types.Builtin(TypeTag::Bool), types.Builtin(TypeTag::Int),
                                 types.Builtin(TypeTag::Float), types.Builtin(TypeTag::String),
                                 types.Builtin(TypeTag::Object)};
        for (const StructDef& s : types.Structs()) {
            list.push_back(types.UserType(s.name));
        }
        for (const EnumDef& e : types.Enums()) {
            list.push_back(types.UserType(e.name));
        }
        return list;
    };
    const auto cycleSelType = [&]() {
        const std::vector<TypeId> list = selectableTypes();
        std::size_t idx = 0;
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (list[i] == selTypeId) {
                idx = i;
            }
        }
        selTypeId = list[(idx + 1) % list.size()];
    };
    // Fields accumulated by the type editor for the next struct/enum.
    std::vector<StructField> pendingFields;
    BuiltinRegistry builtins;
    RegisterDemoClasses(classes, types);
    RegisterConversionNodes(classes, builtins, types);
    RegisterDemoBuiltins(builtins);
    // Load custom wasm node modules from the app-relative wasm/ directory so
    // any node class bound to "wasm:<export>" runs through the v2 runtime
    // (FR-WASM-1..3). Modules are keyed by export name, independent of the
    // behavior registry, so no rebind is needed on project load.
    {
        std::vector<std::string> wasmErrors;
        const int wasmLoaded = WasmHost::Instance().LoadModulesFromDirectory("wasm", wasmErrors);
        for (const std::string& e : wasmErrors) {
            std::printf("wasm: %s\n", e.c_str());
        }
        if (wasmLoaded > 0) {
            std::printf("wasm: loaded %d module(s)\n", wasmLoaded);
        }
    }
    NodeId entry = BuildDemoGraph(*project.graph, classes);
    std::string editContext = "main";

    render::Canvas canvas;
    canvas.SetView(render::Vec2{session.panX, session.panY}, session.zoom);

    // Interaction state (declared early so panel callbacks can read the
    // selection). rebuildPalette is a legacy no-op hook: the spawn menu
    // rebuilds from the live registry every time it opens, so class
    // changes need no explicit refresh. Callers keep the guarded call.
    InteractionFsm fsm;
    int funcCount = 0;
    std::function<void()> rebuildPalette;
    // One undo history per graph (main graph and each function body) so
    // switching edit context preserves each graph's history.
    std::map<Graph*, UndoHistory> histories;
    const auto recordUndo = [&]() { histories[active].Record(*active); };
    const auto undoActive = [&]() { return histories[active].Undo(*active); };
    const auto redoActive = [&]() { return histories[active].Redo(*active); };
    const render::MeasureTextFn measure = [&painter](const std::string& s, float size) {
        return painter.MeasureText(s, size);
    };

    // Re-registers all runtime behaviors after a load (or on demand). The
    // builtins registry is reset first so nothing stale survives.
    const auto rebindBehaviors = [&]() {
        builtins = BuiltinRegistry{};
        RegisterDemoBuiltins(builtins);
        RegisterConversionNodes(classes, builtins, types);
        for (const StructDef& s : types.Structs()) {
            RegisterStructNodes(classes, builtins, types, s);
        }
        for (const auto& fp : functions.All()) {
            RegisterFunctionNodes(classes, builtins, types, *fp);
        }
        for (const VariableDef& v : project.variables) {
            RegisterVariableNodes(classes, builtins, types, v);
        }
    };

    // Applies an alignment/distribution to the selection using the current
    // layout, then writes the new positions back to the nodes.
    const auto applyAlign = [&](bool distribute, AlignMode mode, bool horizontal) {
        const std::vector<NodeId>& sel = fsm.Selection();
        if (sel.size() < 2) {
            return;
        }
        recordUndo();
        const render::GraphLayout layout = render::ComputeGraphLayout(*active, classes, measure);
        std::vector<NodeBox> boxes;
        for (NodeId id : sel) {
            const render::NodeLayout* nl = layout.FindNode(id);
            if (nl != nullptr) {
                boxes.push_back(NodeBox{id, nl->x, nl->y, nl->w, nl->h});
            }
        }
        const std::vector<NodePos> result =
            distribute ? ComputeDistribute(boxes, horizontal) : ComputeAlign(boxes, mode);
        for (const NodePos& p : result) {
            Node* n = active->FindNode(p.id);
            if (n != nullptr) {
                n->x = p.x;
                n->y = p.y;
            }
        }
    };

    // Debug state: breakpoints and a persistent runtime when stepping.
    std::vector<NodeId> breakpoints;
    std::unique_ptr<Runtime> debug;
    const Runtime::LogFn debugLog = [](const std::string& m) { std::printf("[dbg] %s\n", m.c_str()); };

    // Retained UI panels (positioned once against the initial window size).
    bool runRequested = false;
    std::vector<std::unique_ptr<ui::Widget>> panels;
    const float initW = static_cast<float>(window.GetWidth());
    const float initH = static_cast<float>(window.GetHeight());
    // The comment box whose text is being edited (INVALID_ID = none). Set on a
    // comment click, cleared on any other canvas click; the comment editor
    // panel tracks it.
    CommentId selectedComment = INVALID_ID;

    // Left panels stack top-down with a running cursor so they never
    // overlap regardless of font size or how many rows each one needs.
    float leftY = 8.0f;

    // Authoring panels hide behind toolbar toggles so the default view is
    // just the canvas plus a small toolbar (v1-style chrome). Node ops
    // (collapse/expand/align, Edit Fn) live in the node right-click menu.
    bool showVars = session.showVars;
    bool showTypes = session.showTypes;
    bool showWasm = session.showWasm;

    // Toolbar (top-left): run controls plus file/panel toggles.
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);

        auto runRow = std::make_unique<ui::Row>(4.0f);
        runRow->Add(std::make_unique<ui::Button>("Run",
                                                 [&runRequested]() { runRequested = true; }));
        runRow->Add(std::make_unique<ui::Button>("Debug", [&]() {
            debug = std::make_unique<Runtime>(*project.graph, types, classes, builtins, debugLog);
            for (NodeId bp : breakpoints) {
                debug->AddBreakpoint(bp);
            }
            debug->Start(entry);
            debug->Run(10000);
        }));
        runRow->Add(std::make_unique<ui::Button>("Step", [&]() {
            if (debug) {
                debug->Step();
            }
        }));
        runRow->Add(std::make_unique<ui::Button>("Continue", [&]() {
            if (debug) {
                debug->Continue();
                debug->Run(10000);
            }
        }));
        runRow->Add(std::make_unique<ui::Button>("Undo", [&]() {
            if (undoActive()) {
                fsm.ClearSelection();
            }
        }));
        runRow->Add(std::make_unique<ui::Button>("Redo", [&]() {
            if (redoActive()) {
                fsm.ClearSelection();
            }
        }));
        column->Add(std::move(runRow));

        auto fileRow = std::make_unique<ui::Row>(4.0f);
        fileRow->Add(std::make_unique<ui::Button>("Save", [&window]() {
            ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::SaveGraph);
        }));
        fileRow->Add(std::make_unique<ui::Button>("Load", [&window]() {
            ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::OpenGraph);
        }));
        fileRow->Add(std::make_unique<ui::Button>("Comment", [&]() {
            const render::Vec2 c = canvas.ScreenToCanvas(
                render::Vec2{static_cast<float>(window.GetWidth()) * 0.5f,
                             static_cast<float>(window.GetHeight()) * 0.5f});
            Comment cm;
            cm.id = project.comments.empty() ? 1 : project.comments.back().id + 1;
            cm.x = c.x;
            cm.y = c.y;
            cm.text = "Comment";
            project.comments.push_back(cm);
        }));
        fileRow->Add(std::make_unique<ui::Button>("Del Comment", [&]() {
            if (!project.comments.empty()) {
                if (project.comments.back().id == selectedComment) {
                    selectedComment = INVALID_ID;
                }
                project.comments.pop_back();
            }
        }));
        fileRow->Add(std::make_unique<ui::Button>("Vars", [&showVars]() {
            showVars = !showVars;
        }));
        fileRow->Add(std::make_unique<ui::Button>("Types", [&showTypes]() {
            showTypes = !showTypes;
        }));
        fileRow->Add(std::make_unique<ui::Button>("Wasm", [&showWasm]() {
            showWasm = !showWasm;
        }));
        column->Add(std::move(fileRow));

        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        const ui::Size s = raw->Measure(painter, ui::Size{700.0f, 120.0f});
        raw->Arrange(ui::Rect{8.0f, leftY, s.w + 8.0f, s.h + 8.0f});
        leftY += s.h + 16.0f;
        panels.push_back(std::move(panel));
    }

    // Function edit panel: visible only while a function body is being
    // edited (leave with Main, grow/shrink the interface here).
    const std::size_t fnPanelIndex = panels.size();
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto fnRow = std::make_unique<ui::Row>(4.0f);
        fnRow->Add(std::make_unique<ui::Button>("Main", [&]() {
            active = project.graph.get();
            editingDef = nullptr;
            editContext = "main";
            fsm.ClearSelection();
        }));
        fnRow->Add(std::make_unique<ui::Button>("Add In", [&]() {
            if (editingDef != nullptr) {
                AddFunctionParam(*editingDef, false,
                                 "in" + std::to_string(editingDef->inputs.size() + 1),
                                 selTypeId, classes, builtins, types, project);
            }
        }));
        fnRow->Add(std::make_unique<ui::Button>("Add Out", [&]() {
            if (editingDef != nullptr) {
                AddFunctionParam(*editingDef, true,
                                 "out" + std::to_string(editingDef->outputs.size() + 1),
                                 selTypeId, classes, builtins, types, project);
            }
        }));
        fnRow->Add(std::make_unique<ui::Button>("Del In", [&]() {
            if (editingDef != nullptr && !editingDef->inputs.empty()) {
                RemoveFunctionParam(*editingDef, false,
                                    static_cast<int>(editingDef->inputs.size()) - 1, classes,
                                    builtins, types, project);
            }
        }));
        fnRow->Add(std::make_unique<ui::Button>("Del Out", [&]() {
            if (editingDef != nullptr && !editingDef->outputs.empty()) {
                RemoveFunctionParam(*editingDef, true,
                                    static_cast<int>(editingDef->outputs.size()) - 1, classes,
                                    builtins, types, project);
            }
        }));
        ui::Widget* raw = panel.get();
        raw->Add(std::move(fnRow));
        const ui::Size s = raw->Measure(painter, ui::Size{600.0f, 60.0f});
        raw->Arrange(ui::Rect{8.0f, leftY, s.w + 8.0f, s.h + 8.0f});
        leftY += s.h + 16.0f;
        panels.push_back(std::move(panel));
    }

    // Palette panel (top-right): one button per node class spawns a node.
    // Function marshalling classes ("<fn> In"/"<fn> Out") are internal, so
    // they are hidden; the function's Call class is kept. Rebuilt whenever a
    // new function is created.
    const auto isMarshallingClass = [&functions](const std::string& name) {
        for (const auto& def : functions.All()) {
            if (name == def->name + " In" || name == def->name + " Out") {
                return true;
            }
        }
        return false;
    };
    // Node creation is a right-click spawn menu (v1 UX): items rebuild
    // from the live registry every time the menu opens, so there is no
    // permanent palette panel and nothing to refresh on class changes.
    PopupMenu spawnMenu;
    PopupMenu actionMenu;
    NodeId actionTarget = INVALID_ID;
    const auto openSpawnMenu = [&](float sx, float sy) {
        spawnMenu = PopupMenu{};
        spawnMenu.open = true;
        const render::Vec2 c = canvas.ScreenToCanvas(render::Vec2{sx, sy});
        spawnMenu.canvasX = c.x;
        spawnMenu.canvasY = c.y;
        std::vector<std::string> categories;
        for (const NodeClass& cls : classes.All()) {
            if (isMarshallingClass(cls.name)) {
                continue;
            }
            bool known = false;
            for (const std::string& cat : categories) {
                if (cat == cls.category) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                categories.push_back(cls.category);
            }
        }
        for (const std::string& cat : categories) {
            spawnMenu.items.push_back(PopupMenu::Item{cat, true, ""});
            for (const NodeClass& cls : classes.All()) {
                if (cls.category == cat && !isMarshallingClass(cls.name)) {
                    spawnMenu.items.push_back(PopupMenu::Item{cls.name, false, cls.name});
                }
            }
        }
        const float w = static_cast<float>(window.GetWidth());
        const float h = static_cast<float>(window.GetHeight());
        spawnMenu.x = (sx + PopupMenu::WIDTH > w) ? w - PopupMenu::WIDTH - 4.0f : sx;
        spawnMenu.y = (sy + spawnMenu.Height() > h) ? h - spawnMenu.Height() - 4.0f : sy;
    };
    const auto openActionMenu = [&](float sx, float sy, NodeId nodeId) {
        actionMenu = PopupMenu{};
        actionMenu.open = true;
        actionTarget = nodeId;
        const char* actions[6] = {"Edit Fn", "Collapse", "Expand",
                                  "Align L", "Align T", "Distribute H"};
        for (const char* action : actions) {
            actionMenu.items.push_back(PopupMenu::Item{action, false, action});
        }
        const float w = static_cast<float>(window.GetWidth());
        const float h = static_cast<float>(window.GetHeight());
        actionMenu.x = (sx + PopupMenu::WIDTH > w) ? w - PopupMenu::WIDTH - 4.0f : sx;
        actionMenu.y = (sy + actionMenu.Height() > h) ? h - actionMenu.Height() - 4.0f : sy;
    };

    // Selection info panel (bottom-left), text updated each frame.
    ui::Label* infoLabel = nullptr;
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);
        auto label = std::make_unique<ui::Label>("No selection");
        infoLabel = label.get();
        column->Add(std::move(label));
        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        raw->Measure(painter, ui::Size{300.0f, 60.0f});
        raw->Arrange(ui::Rect{8.0f, initH - 40.0f, 300.0f, 30.0f});
        panels.push_back(std::move(panel));
    }

    // Property panel (bottom-right): editable fields for the selected node's
    // properties. Rebuilt when the selection changes.
    const auto buildProperties = [&](NodeId nodeId) -> std::unique_ptr<ui::Widget> {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);
        const Node* node = active->FindNode(nodeId);
        const NodeClass* cls = (node != nullptr) ? classes.Find(node->className) : nullptr;
        if (node != nullptr && cls != nullptr && !node->properties.empty()) {
            column->Add(std::make_unique<ui::Label>("Properties: " + node->className));
            // One undo checkpoint per edit session on this panel: the first
            // field change records; later keystrokes coalesce into it.
            auto recorded = std::make_shared<bool>(false);
            for (std::size_t i = 0; i < node->properties.size() && i < cls->properties.size();
                 ++i) {
                const TypeId pt = cls->properties[i].type;
                auto row = std::make_unique<ui::Row>(6.0f);
                row->Add(std::make_unique<ui::Label>(cls->properties[i].name));
                row->Add(std::make_unique<ui::TextField>(
                    ValueToString(node->properties[i]),
                    [&active, &types, &recordUndo, recorded, nodeId, i, pt](const std::string& v) {
                        Node* n = active->FindNode(nodeId);
                        if (n != nullptr && i < n->properties.size()) {
                            if (!*recorded) {
                                recordUndo();
                                *recorded = true;
                            }
                            n->properties[i] = ParseValue(types, pt, v);
                        }
                    }));
                column->Add(std::move(row));
            }
        } else {
            column->Add(std::make_unique<ui::Label>("Select a node"));
        }
        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        const ui::Size s = raw->Measure(painter, ui::Size{260.0f, 400.0f});
        // Sits above the minimap, which owns the bottom-right corner.
        raw->Arrange(ui::Rect{initW - s.w - 16.0f, initH - s.h - 130.0f - 32.0f, s.w + 8.0f,
                              s.h + 8.0f});
        return panel;
    };
    const std::size_t propertyIndex = panels.size();
    panels.push_back(buildProperties(INVALID_ID));
    NodeId shownNode = INVALID_ID;

    // Comment editor panel (bottom-left, above the info label): a text field to
    // rename the selected comment box (FR-UX-4). Rebuilt when the selected
    // comment changes. Comment text lives outside the graph, so edits are not
    // undo-tracked (same limitation as variable/comment definitions).
    const auto buildCommentPanel = [&](CommentId cid) -> std::unique_ptr<ui::Widget> {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto row = std::make_unique<ui::Row>(6.0f);
        Comment* cm = nullptr;
        for (Comment& c : project.comments) {
            if (c.id == cid) {
                cm = &c;
                break;
            }
        }
        if (cm != nullptr) {
            row->Add(std::make_unique<ui::Label>("Comment"));
            row->Add(std::make_unique<ui::TextField>(
                cm->text,
                [&project, cid](const std::string& v) {
                    for (Comment& c : project.comments) {
                        if (c.id == cid) {
                            c.text = v;
                            break;
                        }
                    }
                },
                160.0f));
        } else {
            row->Add(std::make_unique<ui::Label>("Select a comment"));
        }
        ui::Widget* raw = panel.get();
        raw->Add(std::move(row));
        const ui::Size s = raw->Measure(painter, ui::Size{260.0f, 40.0f});
        raw->Arrange(ui::Rect{8.0f, initH - 84.0f, s.w + 8.0f, s.h + 8.0f});
        return panel;
    };
    const std::size_t commentIndex = panels.size();
    panels.push_back(buildCommentPanel(INVALID_ID));
    CommentId shownComment = INVALID_ID;

    // Search panel (top-center): typing centers the view on matching nodes.
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto row = std::make_unique<ui::Row>(6.0f);
        row->Add(std::make_unique<ui::Label>("Find"));
        row->Add(std::make_unique<ui::TextField>("", [&](const std::string& query) {
            const std::vector<NodeId> matches = SearchNodes(*active, query);
            if (matches.empty()) {
                return;
            }
            const render::GraphLayout layout =
                render::ComputeGraphLayout(*active, classes, measure);
            std::vector<NodeBox> boxes;
            for (NodeId id : matches) {
                const render::NodeLayout* nl = layout.FindNode(id);
                if (nl != nullptr) {
                    boxes.push_back(NodeBox{id, nl->x, nl->y, nl->w, nl->h});
                }
            }
            Bounds b;
            if (!ComputeBounds(boxes, b)) {
                return;
            }
            const render::Vec2 worldCenter{(b.minX + b.maxX) * 0.5f, (b.minY + b.maxY) * 0.5f};
            const render::Vec2 screenNow = canvas.CanvasToScreen(worldCenter);
            canvas.PanByScreenDelta(static_cast<float>(window.GetWidth()) * 0.5f - screenNow.x,
                                    static_cast<float>(window.GetHeight()) * 0.5f - screenNow.y);
        }, 160.0f));
        ui::Widget* raw = panel.get();
        raw->Add(std::move(row));
        const ui::Size s = raw->Measure(painter, ui::Size{240.0f, 40.0f});
        raw->Arrange(ui::Rect{initW * 0.5f - s.w * 0.5f, 8.0f, s.w + 8.0f, s.h + 8.0f});
        panels.push_back(std::move(panel));
    }

    // Variable panel (top-left, toggled by the Vars toolbar button): name
    // field + Add makes a variable of the selected type and registers its
    // Get/Set nodes. Type cycles the scalar type used for both variables
    // and function parameters.
    ui::Label* typeLabel = nullptr;
    const std::size_t varPanelIndex = panels.size();
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto row = std::make_unique<ui::Row>(6.0f);
        auto field = std::make_unique<ui::TextField>("var", [](const std::string&) {}, 100.0f);
        ui::TextField* varField = field.get();
        row->Add(std::make_unique<ui::Label>("Var"));
        row->Add(std::move(field));
        row->Add(std::make_unique<ui::Button>("Add", [&, varField]() {
            const std::string name = varField->Value();
            if (name.empty()) {
                return;
            }
            for (const VariableDef& v : project.variables) {
                if (v.name == name) {
                    return;
                }
            }
            VariableDef def{name, selTypeId};
            project.variables.push_back(def);
            RegisterVariableNodes(classes, builtins, types, def);
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        row->Add(std::make_unique<ui::Button>("Del Var", [&]() {
            // Get/Set classes of the removed variable stay registered (unused).
            if (!project.variables.empty()) {
                project.variables.pop_back();
            }
        }));
        auto tlabel = std::make_unique<ui::Label>("int");
        typeLabel = tlabel.get();
        row->Add(std::make_unique<ui::Button>("Type", [&]() { cycleSelType(); }));
        row->Add(std::move(tlabel));
        ui::Widget* raw = panel.get();
        raw->Add(std::move(row));
        const ui::Size s = raw->Measure(painter, ui::Size{360.0f, 40.0f});
        raw->Arrange(ui::Rect{8.0f, leftY, s.w + 8.0f, s.h + 8.0f});
        leftY += s.h + 16.0f;
        panels.push_back(std::move(panel));
    }

    // Type editor panel (toggled by the Types toolbar button): define
    // struct/enum user types in-app (SRS 2.1 authoring, FR-TYP-6). Add
    // Member accumulates a field of the selected type; Make Struct/Make
    // Enum registers the type (and, for a struct, its Make/Break nodes)
    // and clears the pending fields.
    const std::size_t typePanelIndex = panels.size();
    ui::Label* fieldsLabel = nullptr;
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);
        auto nameRow = std::make_unique<ui::Row>(6.0f);
        auto nameField = std::make_unique<ui::TextField>("Type", [](const std::string&) {}, 100.0f);
        ui::TextField* typeNameField = nameField.get();
        nameRow->Add(std::make_unique<ui::Label>("Name"));
        nameRow->Add(std::move(nameField));
        column->Add(std::move(nameRow));

        auto memberRow = std::make_unique<ui::Row>(6.0f);
        auto memberField =
            std::make_unique<ui::TextField>("field", [](const std::string&) {}, 90.0f);
        ui::TextField* memField = memberField.get();
        memberRow->Add(std::make_unique<ui::Label>("Member"));
        memberRow->Add(std::move(memberField));
        memberRow->Add(std::make_unique<ui::Button>("Add Member", [&, memField]() {
            if (!memField->Value().empty()) {
                pendingFields.push_back(StructField{memField->Value(), selTypeId});
                memField->SetValue("");
            }
        }));
        auto flabel = std::make_unique<ui::Label>("fields: 0");
        fieldsLabel = flabel.get();
        memberRow->Add(std::move(flabel));
        column->Add(std::move(memberRow));

        auto makeRow = std::make_unique<ui::Row>(6.0f);
        makeRow->Add(std::make_unique<ui::Button>("Make Struct", [&, typeNameField]() {
            if (typeNameField->Value().empty() || pendingFields.empty()) {
                return;
            }
            StructDef def;
            def.name = typeNameField->Value();
            def.fields = pendingFields;
            types.DefineStruct(def);
            RegisterStructNodes(classes, builtins, types, def);
            pendingFields.clear();
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        makeRow->Add(std::make_unique<ui::Button>("Make Enum", [&, typeNameField]() {
            if (typeNameField->Value().empty() || pendingFields.empty()) {
                return;
            }
            EnumDef def;
            def.name = typeNameField->Value();
            for (const StructField& f : pendingFields) {
                def.values.push_back(f.name);
            }
            types.DefineEnum(def);
            pendingFields.clear();
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        column->Add(std::move(makeRow));

        // Edit an existing user type (FR-TYP-4 + type deletion): the Name field
        // targets an already-defined type. Add/Del Field grows or shrinks a
        // struct and re-syncs its Make/Break instances across all graphs; Del
        // Type drops the definition. Deleted struct Make/Break classes stay
        // registered as orphans (same behavior as Del Var's Get/Set).
        auto editRow = std::make_unique<ui::Row>(6.0f);
        editRow->Add(std::make_unique<ui::Button>("Add Field", [&, typeNameField, memField]() {
            if (typeNameField->Value().empty() || memField->Value().empty()) {
                return;
            }
            if (types.FindStruct(typeNameField->Value()) == nullptr) {
                return;
            }
            AddStructField(types, typeNameField->Value(), memField->Value(), selTypeId, classes,
                           builtins, project);
            memField->SetValue("");
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        editRow->Add(std::make_unique<ui::Button>("Del Field", [&, typeNameField]() {
            const StructDef* s = types.FindStruct(typeNameField->Value());
            if (s == nullptr || s->fields.empty()) {
                return;
            }
            RemoveStructField(types, typeNameField->Value(),
                              static_cast<int>(s->fields.size()) - 1, classes, builtins, project);
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        editRow->Add(std::make_unique<ui::Button>("Del Type", [&, typeNameField]() {
            const std::string name = typeNameField->Value();
            bool removed = types.RemoveStruct(name);
            if (!removed) {
                removed = types.RemoveEnum(name);
            }
            if (removed) {
                // Fall back to a safe scalar in case the deleted type was the
                // current selection.
                selTypeId = types.Builtin(TypeTag::Int);
                if (rebuildPalette) {
                    rebuildPalette();
                }
            }
        }));
        column->Add(std::move(editRow));

        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        const ui::Size s = raw->Measure(painter, ui::Size{360.0f, 120.0f});
        raw->Arrange(ui::Rect{8.0f, leftY, s.w + 8.0f, s.h + 8.0f});
        leftY += s.h + 16.0f;
        panels.push_back(std::move(panel));
    }

    // Wasm node editor panel (top-left, under Type): author a node class
    // bound to a wasm module export (execFn "wasm:<name>"). Add In/Add Out
    // accumulate pins of the selected type; Make Wasm Node registers the
    // class (export name = class name) and it lands in the palette. The
    // module itself comes from wasm/*.wasm; Reload Wasm re-scans that
    // directory so an externally rebuilt module is picked up without a
    // restart (FR-WASM-1). Toggled by the Wasm toolbar button.
    std::vector<PinDef> pendingPins;
    ui::Label* pinsLabel = nullptr;
    const std::size_t wasmPanelIndex = panels.size();
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);

        auto nameRow = std::make_unique<ui::Row>(6.0f);
        auto nameField =
            std::make_unique<ui::TextField>("WasmNode", [](const std::string&) {}, 110.0f);
        ui::TextField* wasmNameField = nameField.get();
        nameRow->Add(std::make_unique<ui::Label>("Wasm"));
        nameRow->Add(std::move(nameField));
        column->Add(std::move(nameRow));

        auto pinRow = std::make_unique<ui::Row>(6.0f);
        auto pinField = std::make_unique<ui::TextField>("pin", [](const std::string&) {}, 90.0f);
        ui::TextField* pinNameField = pinField.get();
        pinRow->Add(std::move(pinField));
        // Shared by Add In / Add Out; captures main-scope state by reference.
        const auto addPin = [&, pinNameField](PinDirection dir) {
            if (pinNameField->Value().empty()) {
                return;
            }
            pendingPins.push_back(PinDef{dir, selTypeId, pinNameField->Value()});
            pinNameField->SetValue("");
        };
        pinRow->Add(std::make_unique<ui::Button>(
            "Add In", [addPin]() { addPin(PinDirection::Input); }));
        pinRow->Add(std::make_unique<ui::Button>(
            "Add Out", [addPin]() { addPin(PinDirection::Output); }));
        auto plabel = std::make_unique<ui::Label>("pins: 0");
        pinsLabel = plabel.get();
        pinRow->Add(std::move(plabel));
        column->Add(std::move(pinRow));

        auto makeRow = std::make_unique<ui::Row>(6.0f);
        makeRow->Add(std::make_unique<ui::Button>("Make Wasm Node", [&, wasmNameField]() {
            const std::string name = wasmNameField->Value();
            if (name.empty() || pendingPins.empty()) {
                return;
            }
            RegisterWasmNodeClass(classes, name, "CustomObject", pendingPins);
            if (!WasmHost::Instance().HasFunction(name)) {
                std::printf("wasm: no loaded module exports '%s' yet; put a module in wasm/"
                            " and press Reload Wasm\n",
                            name.c_str());
            }
            pendingPins.clear();
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        makeRow->Add(std::make_unique<ui::Button>("Reload Wasm", [&]() {
            std::vector<std::string> errs;
            const int n = WasmHost::Instance().LoadModulesFromDirectory("wasm", errs);
            for (const std::string& e : errs) {
                std::printf("wasm: %s\n", e.c_str());
            }
            std::printf("wasm: reloaded %d module(s)\n", n);
        }));
        column->Add(std::move(makeRow));

        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        const ui::Size s = raw->Measure(painter, ui::Size{360.0f, 100.0f});
        raw->Arrange(ui::Rect{8.0f, leftY, s.w + 8.0f, s.h + 8.0f});
        leftY += s.h + 16.0f;
        panels.push_back(std::move(panel));
    }

    // Per-panel visibility, refreshed every frame: the authoring panels
    // follow their toolbar toggles and the function panel shows only
    // while a function body is being edited.
    std::vector<char> panelShown(panels.size(), 1);

    // Save/load the whole project. Load resets the registries in place (so
    // references stay valid), re-imports, then rebinds behaviors and rebuilds
    // dependent UI. The current path feeds the session file so the next
    // start reopens the same project.
    std::string currentProjectPath = session.projectPath;
    const auto saveProject = [&](const std::string& path) {
        SaveProjectFile(path, project);
        currentProjectPath = path;
    };
    const auto loadProject = [&](const std::string& path) {
        types.Clear();
        classes.Clear();
        functions.Clear();
        project.variables.clear();
        project.comments.clear();
        selectedComment = INVALID_ID;
        *project.graph = Graph(types);
        std::vector<std::string> errors;
        LoadProjectFile(path, project, errors);
        for (const std::string& e : errors) {
            std::printf("[load] %s\n", e.c_str());
        }
        rebindBehaviors();
        active = project.graph.get();
        editingDef = nullptr;
        editContext = "main";
        entry = FindByClass(*project.graph, "EventBegin");
        fsm.ClearSelection();
        breakpoints.clear();
        shownNode = INVALID_ID;
        histories.clear();
        pendingFields.clear();
        pendingPins.clear();
        currentProjectPath = path;
        if (rebuildPalette) {
            rebuildPalette();
        }
    };

    // Reopen the project from the previous session (falls back to the
    // demo graph when the file is gone).
    if (!currentProjectPath.empty()) {
        std::ifstream probe(currentProjectPath, std::ios::binary);
        if (probe.is_open()) {
            probe.close();
            loadProject(currentProjectPath);
        } else {
            currentProjectPath.clear();
        }
    }

    std::vector<EditorInputEvent> events;
    bool panning = false;
    // Set once the right button moves while held; suppresses the
    // right-click menus after a pan.
    bool rightDragged = false;
    bool dragRecorded = false;
    float lastX = 0.0f;
    float lastY = 0.0f;
    // Comment drag: the box being moved and the nodes it carries with it.
    CommentId draggingComment = INVALID_ID;
    float commentLastX = 0.0f;
    float commentLastY = 0.0f;
    std::vector<NodeId> commentGroup;

    // Recenters the canvas on the world point a minimap click maps to.
    // Returns true when the click landed inside the minimap panel.
    const auto minimapClick = [&](float sx, float sy) -> bool {
        const float w = static_cast<float>(window.GetWidth());
        const float h = static_cast<float>(window.GetHeight());
        const ViewRect panel = MinimapPanelRect(w, h);
        if (sx < panel.x || sx > panel.x + panel.w || sy < panel.y || sy > panel.y + panel.h) {
            return false;
        }
        const render::GraphLayout lay = render::ComputeGraphLayout(*active, classes, measure);
        std::vector<NodeBox> boxes;
        for (const render::NodeLayout& nl : lay.Nodes()) {
            boxes.push_back(NodeBox{nl.id, nl.x, nl.y, nl.w, nl.h});
        }
        Bounds content;
        if (!ComputeBounds(boxes, content)) {
            return false;
        }
        const render::Vec2 tl = canvas.ScreenToCanvas(render::Vec2{0.0f, 0.0f});
        const render::Vec2 br = canvas.ScreenToCanvas(render::Vec2{w, h});
        const MinimapFit fit = ComputeMinimap(content, panel, Bounds{tl.x, tl.y, br.x, br.y});
        if (fit.scale <= 0.0f) {
            return false;
        }
        const render::Vec2 world{(sx - fit.offsetX) / fit.scale, (sy - fit.offsetY) / fit.scale};
        const render::Vec2 now = canvas.CanvasToScreen(world);
        canvas.PanByScreenDelta(w * 0.5f - now.x, h * 0.5f - now.y);
        return true;
    };

    for (;;) {
        if (!window.PumpEvents(events)) {
            break;
        }
        FileDialogResult dialog;
        while (PollFileDialogResult(dialog)) {
            if (!dialog.accepted) {
                continue;
            }
            if (dialog.type == FileDialogType::SaveGraph) {
                saveProject(dialog.path);
            } else {
                loadProject(dialog.path);
            }
        }
        const float screenW = static_cast<float>(window.GetWidth());
        const float screenH = static_cast<float>(window.GetHeight());

        // The active graph this frame (main graph, or a function body while
        // editing a function). Rebinds each iteration so context switches take
        // effect immediately.
        Graph& graph = *active;
        const bool editingMain = (active == project.graph.get());

        // Layout from current node positions; hit testing reads it.
        const render::GraphLayout layout = render::ComputeGraphLayout(graph, classes, measure);

        panelShown[varPanelIndex] = showVars ? 1 : 0;
        panelShown[typePanelIndex] = showTypes ? 1 : 0;
        panelShown[wasmPanelIndex] = showWasm ? 1 : 0;
        panelShown[fnPanelIndex] = (editingDef != nullptr) ? 1 : 0;

        for (const EditorInputEvent& e : events) {
            // An open popup menu is modal: it consumes every event.
            if (spawnMenu.open || actionMenu.open) {
                PopupMenu& menu = spawnMenu.open ? spawnMenu : actionMenu;
                if (e.type == EditorInputType::MouseMove) {
                    menu.hover = menu.IndexAt(e.x, e.y);
                } else if (e.type == EditorInputType::MouseWheel) {
                    menu.ScrollBy(e.wheelDelta);
                } else if (e.type == EditorInputType::MouseDown
                           && e.button == EditorMouseButton::Left) {
                    const int index = menu.IndexAt(e.x, e.y);
                    std::string picked;
                    if (index >= 0 && !menu.items[static_cast<std::size_t>(index)].header) {
                        picked = menu.items[static_cast<std::size_t>(index)].tag;
                    }
                    const bool wasSpawn = spawnMenu.open;
                    spawnMenu.open = false;
                    actionMenu.open = false;
                    if (picked.empty()) {
                        // Clicked outside or on a header: just close.
                    } else if (wasSpawn) {
                        const NodeClass* cls = classes.Find(picked);
                        if (cls != nullptr) {
                            recordUndo();
                            graph.AddNode(*cls, menu.canvasX, menu.canvasY);
                        }
                    } else if (picked == "Edit Fn") {
                        const Node* n = graph.FindNode(actionTarget);
                        FunctionDef* def =
                            (n != nullptr) ? functions.Find(n->className) : nullptr;
                        if (def != nullptr) {
                            active = def->body.get();
                            editingDef = def;
                            editContext = def->name;
                            fsm.ClearSelection();
                        }
                    } else if (picked == "Collapse") {
                        std::vector<NodeId> ids = fsm.Selection();
                        bool targetSelected = false;
                        for (NodeId id : ids) {
                            if (id == actionTarget) {
                                targetSelected = true;
                            }
                        }
                        if (!targetSelected) {
                            ids.assign(1, actionTarget);
                        }
                        const std::string name = "Func" + std::to_string(++funcCount);
                        recordUndo();
                        if (CollapseSelection(graph, types, classes, builtins, functions, ids,
                                              name)
                            != INVALID_ID) {
                            fsm.ClearSelection();
                        }
                    } else if (picked == "Expand") {
                        recordUndo();
                        if (ExpandCall(graph, types, classes, functions, actionTarget)) {
                            fsm.ClearSelection();
                        }
                    } else if (picked == "Align L") {
                        applyAlign(false, AlignMode::Left, false);
                    } else if (picked == "Align T") {
                        applyAlign(false, AlignMode::Top, false);
                    } else if (picked == "Distribute H") {
                        applyAlign(true, AlignMode::Left, true);
                    }
                } else if (e.type == EditorInputType::MouseDown
                           || (e.type == EditorInputType::KeyDown
                               && e.key == EditorKey::Escape)) {
                    spawnMenu.open = false;
                    actionMenu.open = false;
                }
                continue;
            }

            const ui::Event ue = Translate(e);
            bool uiHandled = false;
            for (std::size_t i = 0; i < panels.size(); ++i) {
                if (panelShown[i] != 0 && panels[i]->OnEvent(ue)) {
                    uiHandled = true;
                    break;
                }
            }
            if (uiHandled) {
                continue;
            }
            const render::Vec2 cp = canvas.ScreenToCanvas(render::Vec2{e.x, e.y});
            // Minimap click navigates the view (screen-space hit).
            if (e.type == EditorInputType::MouseDown && e.button == EditorMouseButton::Left
                && minimapClick(e.x, e.y)) {
                continue;
            }
            if (e.type == EditorInputType::MouseDown && e.button == EditorMouseButton::Left
                && e.shift) {
                // Shift-click a node toggles a breakpoint.
                const NodeId hit = HitTestNode(layout, cp.x, cp.y);
                if (hit != INVALID_ID) {
                    bool removed = false;
                    for (std::size_t k = 0; k < breakpoints.size(); ++k) {
                        if (breakpoints[k] == hit) {
                            breakpoints.erase(breakpoints.begin()
                                              + static_cast<std::ptrdiff_t>(k));
                            removed = true;
                            break;
                        }
                    }
                    if (!removed) {
                        breakpoints.push_back(hit);
                    }
                }
            } else if (e.type == EditorInputType::MouseDown
                       && e.button == EditorMouseButton::Left) {
                dragRecorded = false;
                // A click inside a comment box (and not on a node) drags the
                // comment together with the nodes it encloses.
                CommentId hitComment = INVALID_ID;
                if (editingMain && HitTestNode(layout, cp.x, cp.y) == INVALID_ID) {
                    for (const Comment& c : project.comments) {
                        if (cp.x >= c.x && cp.x <= c.x + c.w && cp.y >= c.y
                            && cp.y <= c.y + c.h) {
                            hitComment = c.id;
                        }
                    }
                }
                if (hitComment != INVALID_ID) {
                    recordUndo();
                    selectedComment = hitComment;
                    draggingComment = hitComment;
                    commentLastX = cp.x;
                    commentLastY = cp.y;
                    commentGroup.clear();
                    for (const Comment& c : project.comments) {
                        if (c.id != hitComment) {
                            continue;
                        }
                        std::vector<NodeBox> boxes;
                        for (const render::NodeLayout& nl : layout.Nodes()) {
                            boxes.push_back(NodeBox{nl.id, nl.x, nl.y, nl.w, nl.h});
                        }
                        commentGroup = NodesInRect(boxes, ViewRect{c.x, c.y, c.w, c.h});
                    }
                } else {
                    selectedComment = INVALID_ID;
                    fsm.OnMouseDown(cp.x, cp.y, graph, layout);
                }
            } else if (e.type == EditorInputType::MouseUp && e.button == EditorMouseButton::Left) {
                draggingComment = INVALID_ID;
                // A link drag creates/replaces a link on release: checkpoint
                // the graph just before it lands. If the pins can't connect
                // directly but a scalar converter exists, drop one between
                // them instead.
                if (fsm.IsDraggingLink()) {
                    recordUndo();
                    const PinId src = fsm.DragLinkPin();
                    const PinId dst = HitTestPin(layout, cp.x, cp.y, 10.0f);
                    if (dst != INVALID_ID && !graph.CanConnect(src, dst)) {
                        InsertConversion(graph, types, classes, src, dst);
                    }
                }
                fsm.OnMouseUp(cp.x, cp.y, graph, layout);
            } else if (e.type == EditorInputType::MouseDown
                       && e.button == EditorMouseButton::Right) {
                panning = true;
                rightDragged = false;
                lastX = e.x;
                lastY = e.y;
            } else if (e.type == EditorInputType::MouseUp
                       && e.button == EditorMouseButton::Right) {
                panning = false;
                // A right click that never dragged opens a menu (v1 UX):
                // the node action menu over a node, else the spawn menu.
                if (!rightDragged) {
                    const NodeId hit = HitTestNode(layout, cp.x, cp.y);
                    if (hit != INVALID_ID) {
                        openActionMenu(e.x, e.y, hit);
                    } else {
                        openSpawnMenu(e.x, e.y);
                    }
                }
            } else if (e.type == EditorInputType::MouseMove) {
                if (panning) {
                    if (std::fabs(e.x - lastX) + std::fabs(e.y - lastY) > 3.0f) {
                        rightDragged = true;
                    }
                    canvas.PanByScreenDelta(e.x - lastX, e.y - lastY);
                    lastX = e.x;
                    lastY = e.y;
                } else if (draggingComment != INVALID_ID) {
                    const float dx = cp.x - commentLastX;
                    const float dy = cp.y - commentLastY;
                    for (Comment& c : project.comments) {
                        if (c.id == draggingComment) {
                            c.x += dx;
                            c.y += dy;
                        }
                    }
                    for (NodeId id : commentGroup) {
                        Node* n = graph.FindNode(id);
                        if (n != nullptr) {
                            n->x += dx;
                            n->y += dy;
                        }
                    }
                    commentLastX = cp.x;
                    commentLastY = cp.y;
                } else {
                    // Checkpoint once, at the first actual node-drag move, so
                    // the pre-drag positions are captured (a plain click that
                    // never moves records nothing).
                    if (fsm.GetState() == InteractionFsm::State::DraggingNodes
                        && !dragRecorded) {
                        recordUndo();
                        dragRecorded = true;
                    }
                    fsm.OnMouseMove(cp.x, cp.y, graph, layout);
                }
            } else if (e.type == EditorInputType::MouseWheel) {
                canvas.ZoomAt(render::Vec2{e.x, e.y}, std::pow(1.1f, e.wheelDelta));
            } else if (e.type == EditorInputType::KeyDown && e.key == EditorKey::Undo) {
                if (undoActive()) {
                    fsm.ClearSelection();
                    shownNode = INVALID_ID;
                }
            } else if (e.type == EditorInputType::KeyDown && e.key == EditorKey::Redo) {
                if (redoActive()) {
                    fsm.ClearSelection();
                    shownNode = INVALID_ID;
                }
            }
        }

        if (runRequested) {
            runRequested = false;
            // Run always executes the main graph, regardless of what is being
            // edited.
            Runtime rt(*project.graph, types, classes, builtins,
                       [](const std::string& m) { std::printf("[run] %s\n", m.c_str()); });
            rt.Start(entry);
            rt.Run(10000);
        }

        if (typeLabel != nullptr) {
            typeLabel->SetText(types.TypeName(selTypeId));
        }
        if (fieldsLabel != nullptr) {
            fieldsLabel->SetText("fields: " + std::to_string(pendingFields.size()));
        }
        if (pinsLabel != nullptr) {
            pinsLabel->SetText("pins: " + std::to_string(pendingPins.size()));
        }

        if (infoLabel != nullptr) {
            const std::vector<NodeId>& sel = fsm.Selection();
            const std::string ctx = "[" + editContext + "] ";
            if (sel.empty()) {
                infoLabel->SetText(ctx + "No selection");
            } else {
                const Node* first = graph.FindNode(sel[0]);
                const std::string name = (first != nullptr) ? first->className : "";
                infoLabel->SetText(ctx + "Selected " + std::to_string(sel.size()) + ": " + name);
            }
        }

        // Rebuild the property panel when the selected node changes.
        {
            const std::vector<NodeId>& sel = fsm.Selection();
            const NodeId want = sel.empty() ? INVALID_ID : sel[0];
            if (want != shownNode) {
                shownNode = want;
                panels[propertyIndex] = buildProperties(want);
            }
        }

        // Rebuild the comment editor panel when the selected comment changes.
        if (selectedComment != shownComment) {
            shownComment = selectedComment;
            panels[commentIndex] = buildCommentPanel(selectedComment);
        }

        window.BeginFrame(0.12f, 0.12f, 0.13f);
        nvgBeginFrame(vg, screenW, screenH, window.GetPixelRatio());
        DrawGrid(vg, canvas, screenW, screenH);
        if (editingMain) {
            DrawComments(vg, canvas, project.comments);
        }
        render::DrawGraph(vg, canvas, graph, types, layout);
        render::DrawSelection(vg, canvas, layout, fsm.Selection());
        if (fsm.IsDraggingLink()) {
            render::DrawDragLink(vg, canvas, layout, fsm.DragLinkPin(), fsm.DragX(), fsm.DragY());
        }
        for (NodeId bp : breakpoints) {
            render::DrawNodeOutline(vg, canvas, layout, bp, 220, 50, 50, 2.0f);
        }
        if (debug && debug->State() == RunState::Paused) {
            render::DrawNodeOutline(vg, canvas, layout, debug->CurrentNode(), 60, 220, 90, 3.0f);
        }
        DrawMinimap(vg, canvas, layout, screenW, screenH);
        for (std::size_t i = 0; i < panels.size(); ++i) {
            if (panelShown[i] != 0) {
                panels[i]->Paint(painter);
            }
        }
        DrawPopupMenu(vg, spawnMenu);
        DrawPopupMenu(vg, actionMenu);
        nvgEndFrame(vg);
        window.EndFrame();
    }

    session.windowW = window.GetWidth();
    session.windowH = window.GetHeight();
    session.maximized = window.IsMaximized();
    session.panX = canvas.Pan().x;
    session.panY = canvas.Pan().y;
    session.zoom = canvas.Zoom();
    session.projectPath = currentProjectPath;
    session.showVars = showVars;
    session.showTypes = showTypes;
    session.showWasm = showWasm;
    SaveSession(session);

    DestroyPlatformNVGContext(vg);
    window.Shutdown();
    return 0;
}
