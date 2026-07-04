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
#include "exec/StructNodes.h"
#include "exec/VariableNodes.h"
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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace gau;

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
    const ViewRect panel{screenW * 0.5f - 120.0f, screenH - 150.0f, 240.0f, 130.0f};
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
}

int main()
{
    PlatformWindow window;
    if (!window.Init("GAU v2", 1280, 800)) {
        return 1;
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
    BuiltinRegistry builtins;
    RegisterDemoClasses(classes, types);
    RegisterConversionNodes(classes, builtins, types);
    RegisterDemoBuiltins(builtins);
    NodeId entry = BuildDemoGraph(*project.graph, classes);
    std::string editContext = "main";

    render::Canvas canvas;

    // Interaction state (declared early so panel callbacks can read the
    // selection and rebuild the palette).
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
    int spawnCount = 0;
    std::vector<std::unique_ptr<ui::Widget>> panels;
    const float initW = static_cast<float>(window.GetWidth());
    const float initH = static_cast<float>(window.GetHeight());

    // Run panel (top-left).
    {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);
        column->Add(std::make_unique<ui::Button>("Run",
                                                 [&runRequested]() { runRequested = true; }));
        column->Add(std::make_unique<ui::Button>("Debug", [&]() {
            debug = std::make_unique<Runtime>(*project.graph, types, classes, builtins, debugLog);
            for (NodeId bp : breakpoints) {
                debug->AddBreakpoint(bp);
            }
            debug->Start(entry);
            debug->Run(10000);
        }));
        column->Add(std::make_unique<ui::Button>("Step", [&]() {
            if (debug) {
                debug->Step();
            }
        }));
        column->Add(std::make_unique<ui::Button>("Continue", [&]() {
            if (debug) {
                debug->Continue();
                debug->Run(10000);
            }
        }));
        column->Add(std::make_unique<ui::Button>("Collapse", [&]() {
            const std::vector<NodeId> sel = fsm.Selection();
            if (sel.empty()) {
                return;
            }
            const std::string name = "Func" + std::to_string(++funcCount);
            recordUndo();
            const NodeId call =
                CollapseSelection(*active, types, classes, builtins, functions, sel, name);
            if (call != INVALID_ID) {
                fsm.ClearSelection();
                if (rebuildPalette) {
                    rebuildPalette();
                }
            }
        }));
        column->Add(std::make_unique<ui::Button>("Expand", [&]() {
            const std::vector<NodeId>& sel = fsm.Selection();
            if (sel.empty()) {
                return;
            }
            recordUndo();
            if (ExpandCall(*active, types, classes, functions, sel[0])) {
                fsm.ClearSelection();
            }
        }));
        column->Add(std::make_unique<ui::Button>(
            "Align L", [&applyAlign]() { applyAlign(false, AlignMode::Left, false); }));
        column->Add(std::make_unique<ui::Button>(
            "Align T", [&applyAlign]() { applyAlign(false, AlignMode::Top, false); }));
        column->Add(std::make_unique<ui::Button>(
            "Distribute H", [&applyAlign]() { applyAlign(true, AlignMode::Left, true); }));
        column->Add(std::make_unique<ui::Button>("Save", [&window]() {
            ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::SaveGraph);
        }));
        column->Add(std::make_unique<ui::Button>("Load", [&window]() {
            ShowGraphFileDialog(window.GetSDLWindow(), FileDialogType::OpenGraph);
        }));
        column->Add(std::make_unique<ui::Button>("Comment", [&]() {
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
        column->Add(std::make_unique<ui::Button>("Undo", [&]() {
            if (undoActive()) {
                fsm.ClearSelection();
            }
        }));
        column->Add(std::make_unique<ui::Button>("Redo", [&]() {
            if (redoActive()) {
                fsm.ClearSelection();
            }
        }));
        // Enter a selected Call node's function body; return to the main graph.
        column->Add(std::make_unique<ui::Button>("Edit Fn", [&]() {
            const std::vector<NodeId>& sel = fsm.Selection();
            if (sel.empty()) {
                return;
            }
            const Node* n = active->FindNode(sel[0]);
            FunctionDef* def = (n != nullptr) ? functions.Find(n->className) : nullptr;
            if (def == nullptr) {
                return;
            }
            active = def->body.get();
            editingDef = def;
            editContext = def->name;
            fsm.ClearSelection();
        }));
        column->Add(std::make_unique<ui::Button>("Main", [&]() {
            active = project.graph.get();
            editingDef = nullptr;
            editContext = "main";
            fsm.ClearSelection();
        }));
        column->Add(std::make_unique<ui::Button>("Add In", [&]() {
            if (editingDef != nullptr) {
                AddFunctionParam(*editingDef, false,
                                 "in" + std::to_string(editingDef->inputs.size() + 1),
                                 types.Builtin(TypeTag::Int), classes, builtins, types, project);
            }
        }));
        column->Add(std::make_unique<ui::Button>("Add Out", [&]() {
            if (editingDef != nullptr) {
                AddFunctionParam(*editingDef, true,
                                 "out" + std::to_string(editingDef->outputs.size() + 1),
                                 types.Builtin(TypeTag::Int), classes, builtins, types, project);
            }
        }));
        column->Add(std::make_unique<ui::Button>("Del In", [&]() {
            if (editingDef != nullptr && !editingDef->inputs.empty()) {
                RemoveFunctionParam(*editingDef, false,
                                    static_cast<int>(editingDef->inputs.size()) - 1, classes,
                                    builtins, types, project);
            }
        }));
        column->Add(std::make_unique<ui::Button>("Del Out", [&]() {
            if (editingDef != nullptr && !editingDef->outputs.empty()) {
                RemoveFunctionParam(*editingDef, true,
                                    static_cast<int>(editingDef->outputs.size()) - 1, classes,
                                    builtins, types, project);
            }
        }));
        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        const ui::Size s = raw->Measure(painter, ui::Size{200.0f, 200.0f});
        raw->Arrange(ui::Rect{8.0f, 8.0f, s.w + 8.0f, s.h + 8.0f});
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
    const auto buildPalette = [&]() -> std::unique_ptr<ui::Widget> {
        auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
        auto column = std::make_unique<ui::Column>(4.0f);
        for (const NodeClass& c : classes.All()) {
            const std::string name = c.name;
            if (isMarshallingClass(name)) {
                continue;
            }
            column->Add(std::make_unique<ui::Button>(
                "+ " + name,
                [&active, &classes, &canvas, &window, &spawnCount, &recordUndo, name]() {
                    const NodeClass* cls = classes.Find(name);
                    if (cls == nullptr) {
                        return;
                    }
                    const render::Vec2 center = canvas.ScreenToCanvas(render::Vec2{
                        static_cast<float>(window.GetWidth()) * 0.5f,
                        static_cast<float>(window.GetHeight()) * 0.5f});
                    const float offset = static_cast<float>(spawnCount % 8) * 24.0f;
                    recordUndo();
                    active->AddNode(*cls, center.x + offset, center.y + offset);
                    ++spawnCount;
                }));
        }
        ui::Widget* raw = panel.get();
        raw->Add(std::move(column));
        const ui::Size s = raw->Measure(painter, ui::Size{240.0f, 400.0f});
        raw->Arrange(ui::Rect{initW - s.w - 16.0f, 8.0f, s.w + 8.0f, s.h + 8.0f});
        return panel;
    };
    const std::size_t paletteIndex = panels.size();
    panels.push_back(buildPalette());
    rebuildPalette = [&panels, &buildPalette, paletteIndex]() {
        panels[paletteIndex] = buildPalette();
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
        raw->Arrange(ui::Rect{initW - s.w - 16.0f, initH - s.h - 16.0f, s.w + 8.0f, s.h + 8.0f});
        return panel;
    };
    const std::size_t propertyIndex = panels.size();
    panels.push_back(buildProperties(INVALID_ID));
    NodeId shownNode = INVALID_ID;

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

    // Variable panel (top-left, under Run): name field + Add makes an int
    // variable and registers its Get/Set nodes into the palette.
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
            VariableDef def{name, types.Builtin(TypeTag::Int)};
            project.variables.push_back(def);
            RegisterVariableNodes(classes, builtins, types, def);
            if (rebuildPalette) {
                rebuildPalette();
            }
        }));
        ui::Widget* raw = panel.get();
        raw->Add(std::move(row));
        const ui::Size s = raw->Measure(painter, ui::Size{260.0f, 40.0f});
        raw->Arrange(ui::Rect{8.0f, 230.0f, s.w + 8.0f, s.h + 8.0f});
        panels.push_back(std::move(panel));
    }

    // Save/load the whole project. Load resets the registries in place (so
    // references stay valid), re-imports, then rebinds behaviors and rebuilds
    // dependent UI.
    const auto saveProject = [&](const std::string& path) { SaveProjectFile(path, project); };
    const auto loadProject = [&](const std::string& path) {
        types.Clear();
        classes.Clear();
        functions.Clear();
        project.variables.clear();
        project.comments.clear();
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
        if (rebuildPalette) {
            rebuildPalette();
        }
    };

    std::vector<EditorInputEvent> events;
    bool panning = false;
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
        const ViewRect panel{w * 0.5f - 120.0f, h - 150.0f, 240.0f, 130.0f};
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

        for (const EditorInputEvent& e : events) {
            const ui::Event ue = Translate(e);
            bool uiHandled = false;
            for (const std::unique_ptr<ui::Widget>& p : panels) {
                if (p->OnEvent(ue)) {
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
                lastX = e.x;
                lastY = e.y;
            } else if (e.type == EditorInputType::MouseUp
                       && e.button == EditorMouseButton::Right) {
                panning = false;
            } else if (e.type == EditorInputType::MouseMove) {
                if (panning) {
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

        window.BeginFrame(0.12f, 0.12f, 0.13f);
        nvgBeginFrame(vg, screenW, screenH, window.GetPixelRatio());
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
        for (const std::unique_ptr<ui::Widget>& p : panels) {
            p->Paint(painter);
        }
        nvgEndFrame(vg);
        window.EndFrame();
    }

    DestroyPlatformNVGContext(vg);
    window.Shutdown();
    return 0;
}
