#pragma once

// Scalar type-conversion nodes (SRS FR-TYP-3). Registers pure nodes that
// convert between bool/int/float/string, and exposes SuggestConversion so
// the editor can offer to insert a converter when a user drags a link
// between mismatched but convertible pins.

#include "Runtime.h"

#include "model/Graph.h"
#include "model/Ids.h"
#include "model/NodeClassV2.h"

#include "core/Type.h"
#include "core/TypeRegistry.h"

#include <string>

namespace gau {

// Registers the built-in scalar converters and their behaviors.
void RegisterConversionNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                             TypeRegistry& types);

// Returns the class name of a converter from `from` to `to`, or an empty
// string when no direct scalar conversion exists (or the tags are equal).
std::string SuggestConversion(TypeTag from, TypeTag to);

// When two pins can't connect directly but a scalar converter exists, spawns
// that converter between them (output -> converter -> input) and returns
// true. Returns false if the pins connect directly, are the same direction,
// or have no matching converter. The converter classes must be registered.
bool InsertConversion(Graph& graph, const TypeRegistry& types, const NodeClassRegistry& classes,
                      PinId a, PinId b);

} // namespace gau
