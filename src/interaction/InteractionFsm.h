#pragma once

// v2 interaction state machine over a Graph + render::GraphLayout, in
// canvas space. Handles node selection/drag, link creation, and
// rubber-band selection. Rendering-agnostic and unit-testable; the app
// converts screen input to canvas coordinates and feeds it here.

#include "model/Graph.h"
#include "model/Ids.h"
#include "render/GraphLayout.h"

#include <vector>

namespace gau {

class InteractionFsm
{
public:
    enum class State
    {
        Idle,
        DraggingNodes,
        DraggingLink,
        RubberBand,
    };

    void OnMouseDown(float canvasX, float canvasY, Graph& graph,
                     const render::GraphLayout& layout);
    void OnMouseMove(float canvasX, float canvasY, Graph& graph,
                     const render::GraphLayout& layout);
    void OnMouseUp(float canvasX, float canvasY, Graph& graph, const render::GraphLayout& layout);

    State GetState() const { return state; }
    const std::vector<NodeId>& Selection() const { return selection; }
    bool IsSelected(NodeId nodeId) const;
    // Drops the current selection (e.g. after its nodes are removed by a
    // collapse). Returns to Idle.
    void ClearSelection()
    {
        selection.clear();
        state = State::Idle;
    }

    // Link-drag preview (for rendering the temporary wire).
    bool IsDraggingLink() const { return state == State::DraggingLink; }
    PinId DragLinkPin() const { return dragPin; }
    float DragX() const { return dragX; }
    float DragY() const { return dragY; }

    // Rubber-band rectangle (for rendering the selection box).
    bool IsRubberBanding() const { return state == State::RubberBand; }
    float BandX0() const { return bandX0; }
    float BandY0() const { return bandY0; }
    float BandX1() const { return bandX1; }
    float BandY1() const { return bandY1; }

private:
    State state = State::Idle;
    std::vector<NodeId> selection;
    float lastX = 0.0f;
    float lastY = 0.0f;
    PinId dragPin = INVALID_ID;
    float dragX = 0.0f;
    float dragY = 0.0f;
    float bandX0 = 0.0f;
    float bandY0 = 0.0f;
    float bandX1 = 0.0f;
    float bandY1 = 0.0f;
};

} // namespace gau
