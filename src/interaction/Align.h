#pragma once

// Node alignment and distribution (SRS FR-UX-3). Pure coordinate math over
// node bounding boxes so it is unit-testable independently of rendering; the
// app feeds boxes from the layout cache and applies the returned positions
// through an undo command.

#include "model/Ids.h"

#include <vector>

namespace gau {

enum class AlignMode
{
    Left,
    Right,
    Top,
    Bottom,
    CenterHorizontal, // align vertical center lines to a common x
    CenterVertical,   // align horizontal center lines to a common y
};

struct NodeBox
{
    NodeId id = INVALID_ID;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct NodePos
{
    NodeId id = INVALID_ID;
    float x = 0.0f;
    float y = 0.0f;
};

// Returns the new top-left position of each box under the alignment. Only the
// aligned axis moves; fewer than two boxes yields their positions unchanged.
std::vector<NodePos> ComputeAlign(const std::vector<NodeBox>& boxes, AlignMode mode);

// Distributes boxes so their centers are evenly spaced between the two
// extreme centers along the axis (horizontal=true spaces along x). The
// extreme boxes stay put; fewer than three boxes yields no change.
std::vector<NodePos> ComputeDistribute(const std::vector<NodeBox>& boxes, bool horizontal);

} // namespace gau
