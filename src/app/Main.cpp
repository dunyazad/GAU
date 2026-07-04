// v2 application entry: wires core/model/exec/ui/render on the v1 platform
// into a minimal runnable editor -- a demo graph the user can pan/zoom and
// a Run button that executes the graph through the v2 runtime. Assembly
// grows into Application/Document/InputRouter in later slices.

#include "platform/PlatformNVG.h"
#include "platform/PlatformWindow.h"

#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/FunctionOps.h"
#include "exec/Runtime.h"
#include "interaction/Align.h"
#include "interaction/HitTest2.h"
#include "interaction/InteractionFsm.h"
#include "model/Function.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "render/GraphLayout.h"
#include "render/GraphRenderer.h"
#include "render/NanoVgPainter.h"
#include "render/RenderCanvas.h"
#include "ui/Widget.h"

#include <nanovg.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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

    TypeRegistry types;
    NodeClassRegistry classes;
    RegisterDemoClasses(classes, types);
    BuiltinRegistry builtins;
    RegisterDemoBuiltins(builtins);
    FunctionRegistry functions;
    Graph graph(types);
    const NodeId entry = BuildDemoGraph(graph, classes);

    render::Canvas canvas;

    // Interaction state (declared early so panel callbacks can read the
    // selection and rebuild the palette).
    InteractionFsm fsm;
    int funcCount = 0;
    std::function<void()> rebuildPalette;
    const render::MeasureTextFn measure = [&painter](const std::string& s, float size) {
        return painter.MeasureText(s, size);
    };

    // Applies an alignment/distribution to the selection using the current
    // layout, then writes the new positions back to the nodes.
    const auto applyAlign = [&](bool distribute, AlignMode mode, bool horizontal) {
        const std::vector<NodeId>& sel = fsm.Selection();
        if (sel.size() < 2) {
            return;
        }
        const render::GraphLayout layout = render::ComputeGraphLayout(graph, classes, measure);
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
            Node* n = graph.FindNode(p.id);
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
            debug = std::make_unique<Runtime>(graph, types, classes, builtins, debugLog);
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
            const NodeId call =
                CollapseSelection(graph, types, classes, builtins, functions, sel, name);
            if (call != INVALID_ID) {
                fsm.ClearSelection();
                if (rebuildPalette) {
                    rebuildPalette();
                }
            }
        }));
        column->Add(std::make_unique<ui::Button>("Expand", [&]() {
            const std::vector<NodeId>& sel = fsm.Selection();
            if (!sel.empty() && ExpandCall(graph, types, classes, functions, sel[0])) {
                fsm.ClearSelection();
            }
        }));
        column->Add(std::make_unique<ui::Button>(
            "Align L", [&applyAlign]() { applyAlign(false, AlignMode::Left, false); }));
        column->Add(std::make_unique<ui::Button>(
            "Align T", [&applyAlign]() { applyAlign(false, AlignMode::Top, false); }));
        column->Add(std::make_unique<ui::Button>(
            "Distribute H", [&applyAlign]() { applyAlign(true, AlignMode::Left, true); }));
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
                "+ " + name, [&graph, &classes, &canvas, &window, &spawnCount, name]() {
                    const NodeClass* cls = classes.Find(name);
                    if (cls == nullptr) {
                        return;
                    }
                    const render::Vec2 center = canvas.ScreenToCanvas(render::Vec2{
                        static_cast<float>(window.GetWidth()) * 0.5f,
                        static_cast<float>(window.GetHeight()) * 0.5f});
                    const float offset = static_cast<float>(spawnCount % 8) * 24.0f;
                    graph.AddNode(*cls, center.x + offset, center.y + offset);
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
        const Node* node = graph.FindNode(nodeId);
        const NodeClass* cls = (node != nullptr) ? classes.Find(node->className) : nullptr;
        if (node != nullptr && cls != nullptr && !node->properties.empty()) {
            column->Add(std::make_unique<ui::Label>("Properties: " + node->className));
            for (std::size_t i = 0; i < node->properties.size() && i < cls->properties.size();
                 ++i) {
                const TypeId pt = cls->properties[i].type;
                auto row = std::make_unique<ui::Row>(6.0f);
                row->Add(std::make_unique<ui::Label>(cls->properties[i].name));
                row->Add(std::make_unique<ui::TextField>(
                    ValueToString(node->properties[i]),
                    [&graph, &types, nodeId, i, pt](const std::string& v) {
                        Node* n = graph.FindNode(nodeId);
                        if (n != nullptr && i < n->properties.size()) {
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

    std::vector<EditorInputEvent> events;
    bool panning = false;
    float lastX = 0.0f;
    float lastY = 0.0f;

    for (;;) {
        if (!window.PumpEvents(events)) {
            break;
        }
        const float screenW = static_cast<float>(window.GetWidth());
        const float screenH = static_cast<float>(window.GetHeight());

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
                fsm.OnMouseDown(cp.x, cp.y, graph, layout);
            } else if (e.type == EditorInputType::MouseUp && e.button == EditorMouseButton::Left) {
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
                } else {
                    fsm.OnMouseMove(cp.x, cp.y, graph, layout);
                }
            } else if (e.type == EditorInputType::MouseWheel) {
                canvas.ZoomAt(render::Vec2{e.x, e.y}, std::pow(1.1f, e.wheelDelta));
            }
        }

        if (runRequested) {
            runRequested = false;
            Runtime rt(graph, types, classes, builtins,
                       [](const std::string& m) { std::printf("[run] %s\n", m.c_str()); });
            rt.Start(entry);
            rt.Run(10000);
        }

        if (infoLabel != nullptr) {
            const std::vector<NodeId>& sel = fsm.Selection();
            if (sel.empty()) {
                infoLabel->SetText("No selection");
            } else {
                const Node* first = graph.FindNode(sel[0]);
                const std::string name = (first != nullptr) ? first->className : "";
                infoLabel->SetText("Selected " + std::to_string(sel.size()) + ": " + name);
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
