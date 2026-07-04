#pragma once

// Auto-generates Get/Set node classes and behaviors for a local variable
// (SRS FR-REU-2). "Get <var>" is a pure node reading the runtime value;
// "Set <var>" is an exec node writing it. Values live in the Runtime and
// persist across exec steps within one run.

#include "Runtime.h"

#include "model/NodeClassV2.h"
#include "model/Variable.h"

#include "core/TypeRegistry.h"

namespace gau {

void RegisterVariableNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                           TypeRegistry& types, const VariableDef& def);

} // namespace gau
