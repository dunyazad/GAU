// v2 interaction state machine.

#include "InteractionFsm.h"

#include "HitTest2.h"

namespace gau {

static const float PIN_HIT_RADIUS = 10.0f;

bool InteractionFsm::IsSelected(NodeId nodeId) const
{
    for (NodeId id : selection) {
        if (id == nodeId) {
            return true;
        }
    }
    return false;
}

void InteractionFsm::OnMouseDown(float canvasX, float canvasY, Graph& graph,
                                 const render::GraphLayout& layout)
{
    (void)graph;
    const PinId pin = HitTestPin(layout, canvasX, canvasY, PIN_HIT_RADIUS);
    if (pin != INVALID_ID) {
        state = State::DraggingLink;
        dragPin = pin;
        dragX = canvasX;
        dragY = canvasY;
        return;
    }

    const NodeId node = HitTestNode(layout, canvasX, canvasY);
    if (node != INVALID_ID) {
        if (!IsSelected(node)) {
            selection.clear();
            selection.push_back(node);
        }
        state = State::DraggingNodes;
        lastX = canvasX;
        lastY = canvasY;
        return;
    }

    selection.clear();
    state = State::RubberBand;
    bandX0 = canvasX;
    bandY0 = canvasY;
    bandX1 = canvasX;
    bandY1 = canvasY;
}

void InteractionFsm::OnMouseMove(float canvasX, float canvasY, Graph& graph,
                                 const render::GraphLayout& layout)
{
    switch (state) {
    case State::DraggingNodes: {
        const float dx = canvasX - lastX;
        const float dy = canvasY - lastY;
        for (NodeId id : selection) {
            Node* node = graph.FindNode(id);
            if (node != nullptr) {
                node->x += dx;
                node->y += dy;
            }
        }
        lastX = canvasX;
        lastY = canvasY;
        break;
    }
    case State::DraggingLink:
        dragX = canvasX;
        dragY = canvasY;
        break;
    case State::RubberBand:
        bandX1 = canvasX;
        bandY1 = canvasY;
        selection = HitTestNodesInRect(layout, bandX0, bandY0, bandX1, bandY1);
        break;
    case State::Idle:
        break;
    }
}

void InteractionFsm::OnMouseUp(float canvasX, float canvasY, Graph& graph,
                               const render::GraphLayout& layout)
{
    if (state == State::DraggingLink) {
        const PinId target = HitTestPin(layout, canvasX, canvasY, PIN_HIT_RADIUS);
        if (target != INVALID_ID && target != dragPin && graph.CanConnect(dragPin, target)) {
            graph.AddLink(dragPin, target);
        }
        dragPin = INVALID_ID;
    }
    state = State::Idle;
}

} // namespace gau
