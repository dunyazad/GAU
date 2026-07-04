#pragma once

// v2 graph-scoped local variable (SRS FR-REU-2). A variable has a name and a
// type; exec/VariableNodes turns each into Get (pure read) and Set (exec
// write) nodes, and the Runtime holds the live value during execution.

#include "core/Type.h"

#include <string>

namespace gau {

struct VariableDef
{
    std::string name;
    TypeId type = INVALID_TYPE;
};

} // namespace gau
