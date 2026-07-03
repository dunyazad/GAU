#include "LinkRenderer.h"
#include "NodeLayoutCache.h"
#include "PinStyle.h"

#include "model/NodeGraph.h"

#include <nanovg.h>

// outX/outY is the output-pin end, inX/inY the input-pin end.
static void DrawLinkCurve(NVGcontext* vg, float outX, float outY, float inX, float inY,
                          NVGcolor color, float width)
{
    const float tangent = LinkTangent(inX - outX);
    nvgBeginPath(vg);
    nvgMoveTo(vg, outX, outY);
    nvgBezierTo(vg, outX + tangent, outY, inX - tangent, inY, inX, inY);
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, width);
    nvgStroke(vg);
}

static float LinkWidthForType(PinType type)
{
    return (type == PinType::Exec) ? 3.0f : 2.0f;
}

static const float LINK_POINT_RADIUS = 5.0f;

void DrawLinks(NVGcontext* vg, const NodeGraph& graph, const NodeLayoutCache& layoutCache)
{
    for (const Link& link : graph.GetLinks()) {
        const PinLayout* fromPin = layoutCache.FindPin(link.fromPinId);
        const PinLayout* toPin = layoutCache.FindPin(link.toPinId);
        if (fromPin == nullptr || toPin == nullptr) {
            continue;
        }
        const NVGcolor color = PinColorForType(fromPin->type);
        const float width = LinkWidthForType(fromPin->type);

        // Curve through the reroute waypoints, one bezier per segment.
        float previousX = fromPin->x;
        float previousY = fromPin->y;
        for (const LinkPoint& point : link.points) {
            DrawLinkCurve(vg, previousX, previousY, point.x, point.y, color, width);
            previousX = point.x;
            previousY = point.y;
        }
        DrawLinkCurve(vg, previousX, previousY, toPin->x, toPin->y, color, width);

        for (const LinkPoint& point : link.points) {
            nvgBeginPath(vg);
            nvgCircle(vg, point.x, point.y, LINK_POINT_RADIUS);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
    }
}

void DrawDraggingLink(NVGcontext* vg, const NodeLayoutCache& layoutCache,
                      PinId fromPinId, float toCanvasX, float toCanvasY)
{
    const PinLayout* pin = layoutCache.FindPin(fromPinId);
    if (pin == nullptr) {
        return;
    }
    if (pin->direction == PinDirection::Output) {
        DrawLinkCurve(vg, pin->x, pin->y, toCanvasX, toCanvasY,
                      PinColorForType(pin->type), LinkWidthForType(pin->type));
    } else {
        DrawLinkCurve(vg, toCanvasX, toCanvasY, pin->x, pin->y,
                      PinColorForType(pin->type), LinkWidthForType(pin->type));
    }
}
