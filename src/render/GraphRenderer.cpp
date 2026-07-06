// v2 graph rendering.

#include "GraphRenderer.h"

#include "core/TypeRegistry.h"

#include <nanovg.h>

#include <string>

namespace gau::render {

static NVGcolor HashColor(const std::string& name)
{
    unsigned int hash = 2166136261u;
    for (char c : name) {
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    }
    const int hue = static_cast<int>(hash % 360u);
    return nvgHSL(static_cast<float>(hue) / 360.0f, 0.6f, 0.6f);
}

static NVGcolor TypeColor(const TypeRegistry& types, TypeId id)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        return nvgRGB(255, 255, 255);
    }
    switch (desc->tag) {
    case TypeTag::Exec:
        return nvgRGB(255, 255, 255);
    case TypeTag::Bool:
        return nvgRGB(140, 0, 0);
    case TypeTag::Int:
        return nvgRGB(30, 200, 160);
    case TypeTag::Float:
        return nvgRGB(160, 250, 60);
    case TypeTag::String:
        return nvgRGB(250, 0, 220);
    case TypeTag::Object:
        return nvgRGB(0, 160, 240);
    default:
        return HashColor(types.TypeName(id));
    }
}

static void DrawLinks(NVGcontext* vg, const Graph& graph, const TypeRegistry& types,
                      const GraphLayout& layout)
{
    for (const Link& link : graph.Links()) {
        const PinLayout* from = layout.FindPin(link.fromPin);
        const PinLayout* to = layout.FindPin(link.toPin);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        const float dx = to->x - from->x;
        const float tangent = (dx < 200.0f && dx > -200.0f) ? 100.0f : dx * 0.5f;
        nvgBeginPath(vg);
        nvgMoveTo(vg, from->x, from->y);
        nvgBezierTo(vg, from->x + tangent, from->y, to->x - tangent, to->y, to->x, to->y);
        nvgStrokeColor(vg, TypeColor(types, from->type));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
    }
}

static void DrawNode(NVGcontext* vg, const Node& node, const TypeRegistry& types,
                     const NodeLayout& nl)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, nl.x, nl.y, nl.w, nl.h, 6.0f);
    nvgFillColor(vg, nvgRGBA(24, 24, 28, 235));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Header.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, nl.x, nl.y, nl.w, 26.0f, 6.0f);
    nvgFillColor(vg, nvgRGB(40, 80, 160));
    nvgFill(vg);

    nvgFontFace(vg, "sans-bold");
    nvgFontSize(vg, 14.0f);
    nvgFillColor(vg, nvgRGB(240, 240, 245));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, nl.x + 8.0f, nl.y + 13.0f, node.className.c_str(), nullptr);

    for (const PinLayout& pin : nl.pins) {
        nvgBeginPath(vg);
        nvgCircle(vg, pin.x, pin.y, 5.0f);
        nvgFillColor(vg, TypeColor(types, pin.type));
        nvgFill(vg);
    }
}

void DrawGraph(NVGcontext* vg, const Canvas& canvas, const Graph& graph,
               const TypeRegistry& types, const GraphLayout& layout)
{
    nvgSave(vg);
    const Vec2 pan = canvas.Pan();
    nvgTranslate(vg, pan.x, pan.y);
    nvgScale(vg, canvas.Zoom(), canvas.Zoom());

    DrawLinks(vg, graph, types, layout);
    for (const Node& node : graph.Nodes()) {
        const NodeLayout* nl = layout.FindNode(node.id);
        if (nl != nullptr) {
            DrawNode(vg, node, types, *nl);
        }
    }

    nvgRestore(vg);
}

static void ApplyCanvas(NVGcontext* vg, const Canvas& canvas)
{
    const Vec2 pan = canvas.Pan();
    nvgTranslate(vg, pan.x, pan.y);
    nvgScale(vg, canvas.Zoom(), canvas.Zoom());
}

void DrawSelection(NVGcontext* vg, const Canvas& canvas, const GraphLayout& layout,
                   const std::vector<NodeId>& selection)
{
    nvgSave(vg);
    ApplyCanvas(vg, canvas);
    for (NodeId id : selection) {
        const NodeLayout* nl = layout.FindNode(id);
        if (nl == nullptr) {
            continue;
        }
        nvgBeginPath(vg);
        nvgRoundedRect(vg, nl->x - 2.0f, nl->y - 2.0f, nl->w + 4.0f, nl->h + 4.0f, 7.0f);
        nvgStrokeColor(vg, nvgRGB(255, 180, 40));
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
    }
    nvgRestore(vg);
}

void DrawNodeOutline(NVGcontext* vg, const Canvas& canvas, const GraphLayout& layout, NodeId node,
                     int r, int g, int b, float width)
{
    const NodeLayout* nl = layout.FindNode(node);
    if (nl == nullptr) {
        return;
    }
    nvgSave(vg);
    ApplyCanvas(vg, canvas);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, nl->x - 3.0f, nl->y - 3.0f, nl->w + 6.0f, nl->h + 6.0f, 8.0f);
    nvgStrokeColor(vg, nvgRGB(static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                              static_cast<unsigned char>(b)));
    nvgStrokeWidth(vg, width);
    nvgStroke(vg);
    nvgRestore(vg);
}

void DrawDragLink(NVGcontext* vg, const Canvas& canvas, const GraphLayout& layout, PinId fromPin,
                  float dragCanvasX, float dragCanvasY)
{
    const PinLayout* from = layout.FindPin(fromPin);
    if (from == nullptr) {
        return;
    }
    nvgSave(vg);
    ApplyCanvas(vg, canvas);
    const float dx = dragCanvasX - from->x;
    const float tangent = (dx < 200.0f && dx > -200.0f) ? 100.0f : dx * 0.5f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, from->x, from->y);
    nvgBezierTo(vg, from->x + tangent, from->y, dragCanvasX - tangent, dragCanvasY, dragCanvasX,
                dragCanvasY);
    nvgStrokeColor(vg, nvgRGBA(230, 230, 230, 200));
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);
    nvgRestore(vg);
}

void DrawRubberBand(NVGcontext* vg, const Canvas& canvas, float x0, float y0, float x1, float y1)
{
    const float minX = x0 < x1 ? x0 : x1;
    const float minY = y0 < y1 ? y0 : y1;
    const float maxX = x0 > x1 ? x0 : x1;
    const float maxY = y0 > y1 ? y0 : y1;
    const Vec2 a = canvas.CanvasToScreen(Vec2{minX, minY});
    const Vec2 b = canvas.CanvasToScreen(Vec2{maxX, maxY});

    nvgBeginPath(vg);
    nvgRect(vg, a.x, a.y, b.x - a.x, b.y - a.y);
    nvgFillColor(vg, nvgRGBA(255, 180, 40, 26));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(255, 180, 40, 200));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}

} // namespace gau::render
