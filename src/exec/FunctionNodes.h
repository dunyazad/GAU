#pragma once

// Auto-generates Entry/Return/Call node classes and behaviors for a
// function definition (SRS FR-REU-1). "<name> In" is the body's input
// source, "<name> Out" is the body's output sink, and "<name>" is the
// callable node placed in other graphs. Calling runs the body in a nested
// Runtime, marshalling inputs in and outputs back out.

#include "Runtime.h"

#include "model/Function.h"
#include "model/NodeClassV2.h"

#include "core/TypeRegistry.h"

namespace gau {

void RegisterFunctionNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                           TypeRegistry& types, const FunctionDef& def);

} // namespace gau
