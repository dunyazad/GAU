#pragma once

// v2 comment/group box (SRS FR-UX-4). A titled rectangle drawn behind nodes;
// nodes whose boxes fall inside it move together when the comment is dragged.
// Pure data; layout/hit-testing live in the interaction/render layers.

#include "Ids.h"

#include <string>

namespace gau {

struct Comment
{
    CommentId id = INVALID_ID;
    float x = 0.0f;
    float y = 0.0f;
    float w = 200.0f;
    float h = 120.0f;
    std::string text;
};

} // namespace gau
