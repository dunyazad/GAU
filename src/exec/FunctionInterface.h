#pragma once

// Function interface editing (SRS FR-REU-1): grow a function's parameter list
// after it exists. Appending a parameter regenerates the Entry/Return/Call
// node classes and appends the matching pin to every existing instance across
// the project, so current graphs stay consistent without relinking. Parameter
// removal is not supported here (it would require dropping pins and their
// links across all instances).

#include "Runtime.h"

#include "model/Function.h"
#include "model/NodeClassV2.h"
#include "model/Project.h"

#include "core/Type.h"
#include "core/TypeRegistry.h"

#include <string>

namespace gau {

// Appends an input (isOutput=false) or output (isOutput=true) parameter to the
// function, regenerates its node classes/behaviors, and appends the matching
// pin to every Entry/Return/Call instance in the project's graphs.
void AddFunctionParam(FunctionDef& def, bool isOutput, const std::string& name, TypeId type,
                      NodeClassRegistry& classes, BuiltinRegistry& builtins, TypeRegistry& types,
                      Project& project);

// Removes the input/output parameter at `index`, dropping the matching pin
// (and any links on it) from every Entry/Return/Call instance, then
// regenerates the classes/behaviors. No-op if the index is out of range.
void RemoveFunctionParam(FunctionDef& def, bool isOutput, int index, NodeClassRegistry& classes,
                         BuiltinRegistry& builtins, TypeRegistry& types, Project& project);

} // namespace gau
