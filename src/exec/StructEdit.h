#pragma once

// Struct field editing (SRS FR-TYP-4): add or remove a field on a struct type
// that graphs already use. Redefines the struct, regenerates its Make/Break
// node classes/behaviors, and reconciles the matching pin on every Make/Break
// instance across the project so current graphs stay consistent without
// relinking. Removal drops the pin (and any links on it) at the given index.

#include "Runtime.h"

#include "model/NodeClassV2.h"
#include "model/Project.h"

#include "core/Type.h"
#include "core/TypeRegistry.h"

#include <string>

namespace gau {

// Appends a field to the named struct, then appends the matching pin (an input
// on Make, an output on Break) to every instance. No-op if no such struct.
void AddStructField(TypeRegistry& types, const std::string& structName,
                    const std::string& fieldName, TypeId fieldType, NodeClassRegistry& classes,
                    BuiltinRegistry& builtins, Project& project);

// Removes the field at `index` from the named struct, dropping the matching pin
// (and any links) from every instance. No-op if the struct or index is invalid.
void RemoveStructField(TypeRegistry& types, const std::string& structName, int index,
                       NodeClassRegistry& classes, BuiltinRegistry& builtins, Project& project);

} // namespace gau
