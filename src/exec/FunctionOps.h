#pragma once

// Collapse/expand operations for functions (SRS FR-REU-1). Collapse folds a
// selected set of nodes into a new function and replaces them with a single
// Call node; expand inlines a Call node back into copies of its function
// body. Both edit the main graph in place. Collapse also generates the
// function's node classes and runtime behaviors, so it lives in the exec
// layer alongside RegisterFunctionNodes.

#include "Runtime.h"

#include "model/Function.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include "core/TypeRegistry.h"

#include <string>
#include <vector>

namespace gau {

// Folds the selected main-graph nodes into a new function `name`, registers
// its classes/behaviors, and replaces the selection with one Call node.
// Returns the Call node id, or INVALID_ID on failure (empty selection,
// missing class, or a name already in use).
NodeId CollapseSelection(Graph& main, TypeRegistry& types, NodeClassRegistry& classes,
                         BuiltinRegistry& builtins, FunctionRegistry& functions,
                         const std::vector<NodeId>& selection, const std::string& name);

// Inlines a Call node back into copies of its function body, rewiring the
// call's boundary links to the body's interior. Returns false if the node is
// not a call to a known function.
bool ExpandCall(Graph& main, TypeRegistry& types, NodeClassRegistry& classes,
                const FunctionRegistry& functions, NodeId callNode);

} // namespace gau
