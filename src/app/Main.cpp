// v2 application entry: wires core/model/exec/ui/render on the v1 platform
// into a minimal runnable editor -- a demo graph the user can pan/zoom and
// a Run button that executes the graph through the v2 runtime. Assembly
// grows into Application/Document/InputRouter in later slices.

#include "platform/PlatformNVG.h"
#include "platform/PlatformWindow.h"

#include "core/TypeRegistry.h"
#include "exec/Builtins.h"
#include "exec/Runtime.h"
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
        break;
    case EditorInputType::TextInput:
        u.type = ui::EventType::Text;
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
    Graph graph(types);
    const NodeId entry = BuildDemoGraph(graph, classes);

    render::Canvas canvas;

    // Retained UI: a Run button in a top-left panel.
    bool runRequested = false;
    auto panel = std::make_unique<ui::Panel>(ui::Color{28, 28, 32, 235});
    auto column = std::make_unique<ui::Column>(4.0f);
    column->Add(std::make_unique<ui::Button>("Run", [&runRequested]() { runRequested = true; }));
    ui::Widget* panelRaw = panel.get();
    panelRaw->Add(std::move(column));
    const ui::Size panelSize = panelRaw->Measure(painter, ui::Size{200.0f, 200.0f});
    panelRaw->Arrange(ui::Rect{8.0f, 8.0f, panelSize.w + 8.0f, panelSize.h + 8.0f});

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

        for (const EditorInputEvent& e : events) {
            const ui::Event ue = Translate(e);
            const bool uiHandled = panelRaw->OnEvent(ue);
            if (uiHandled) {
                continue;
            }
            if (e.type == EditorInputType::MouseDown && e.button == EditorMouseButton::Right) {
                panning = true;
                lastX = e.x;
                lastY = e.y;
            } else if (e.type == EditorInputType::MouseUp
                       && e.button == EditorMouseButton::Right) {
                panning = false;
            } else if (e.type == EditorInputType::MouseMove && panning) {
                canvas.PanByScreenDelta(e.x - lastX, e.y - lastY);
                lastX = e.x;
                lastY = e.y;
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

        const render::MeasureTextFn measure = [&painter](const std::string& s, float size) {
            return painter.MeasureText(s, size);
        };
        const render::GraphLayout layout = render::ComputeGraphLayout(graph, classes, measure);

        window.BeginFrame(0.12f, 0.12f, 0.13f);
        nvgBeginFrame(vg, screenW, screenH, window.GetPixelRatio());
        render::DrawGraph(vg, canvas, graph, types, layout);
        panelRaw->Paint(painter);
        nvgEndFrame(vg);
        window.EndFrame();
    }

    DestroyPlatformNVGContext(vg);
    window.Shutdown();
    return 0;
}
