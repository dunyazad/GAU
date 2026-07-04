#pragma once

// Auto-generates Make/Break node classes and behaviors for a struct type
// (SRS FR-TYP-2): "Make <Struct>" assembles field inputs into a struct
// output; "Break <Struct>" splits a struct input into field outputs.

#include "Runtime.h"

#include "model/NodeClassV2.h"

#include "core/TypeRegistry.h"

namespace gau {

void RegisterStructNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                         TypeRegistry& types, const StructDef& def);

} // namespace gau
