#pragma once

// v2 model identifiers and shared small enums. gau namespace, distinct
// from the v1 model headers so both trees coexist during migration.

#include <cstdint>

namespace gau {

using NodeId = std::uint32_t;
using PinId = std::uint32_t;
using LinkId = std::uint32_t;
using CommentId = std::uint32_t;

constexpr std::uint32_t INVALID_ID = 0;

enum class PinDirection
{
    Input,
    Output,
};

} // namespace gau
