#pragma once

#include "model/GraphTypes.h"

#include <vector>

// Per-frame screen layout of nodes and pins, computed by the render layer
// in canvas coordinates. HitTest (M3) reads this cache instead of
// recomputing layout.

struct PinLayout
{
    PinId pinId = INVALID_ID;
    PinDirection direction = PinDirection::Input;
    PinType type = PinType::Exec;
    // Pin center in canvas coordinates.
    float x = 0.0f;
    float y = 0.0f;
    bool connected = false;
};

struct NodeLayout
{
    NodeId nodeId = INVALID_ID;
    // Canvas-space bounds.
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    std::vector<PinLayout> pins;
};

class NodeLayoutCache
{
public:
    void Clear()
    {
        layouts.clear();
    }

    void Add(NodeLayout layout)
    {
        layouts.push_back(static_cast<NodeLayout&&>(layout));
    }

    const NodeLayout* Find(NodeId nodeId) const
    {
        for (const NodeLayout& layout : layouts) {
            if (layout.nodeId == nodeId) {
                return &layout;
            }
        }
        return nullptr;
    }

    const PinLayout* FindPin(PinId pinId) const
    {
        for (const NodeLayout& layout : layouts) {
            for (const PinLayout& pin : layout.pins) {
                if (pin.pinId == pinId) {
                    return &pin;
                }
            }
        }
        return nullptr;
    }

    const std::vector<NodeLayout>& GetAll() const
    {
        return layouts;
    }

private:
    std::vector<NodeLayout> layouts;
};
