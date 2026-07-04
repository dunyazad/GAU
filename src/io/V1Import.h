#pragma once

// Migrates v1 JSON (custom_nodes.json definitions and saved graphs) into
// the v2 core/model. Functions take JSON text so they are testable without
// files. Errors are appended rather than thrown (no exceptions policy).

#include "core/TypeRegistry.h"
#include "model/Comment.h"
#include "model/Function.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "model/Variable.h"

#include <string>
#include <vector>

namespace gau {

struct ImportCounts
{
    int types = 0;
    int classes = 0;
};

// Loads v1 "types" and "nodeClasses" into the registries. Types load
// first so class pins/properties can reference them.
ImportCounts ImportV1Definitions(const std::string& jsonText, TypeRegistry& types,
                                 NodeClassRegistry& classes, std::vector<std::string>& errors);

// Loads a v1 graph document (nodes + links) into an empty Graph built on
// the given registries. Returns false on a fatal parse error.
bool ImportV1Graph(const std::string& jsonText, Graph& graph, const NodeClassRegistry& classes,
                   TypeRegistry& types, std::vector<std::string>& errors);

// Loads a "functions" array (interface + body graph) into the registry.
// Requires the node classes (including each function's generated
// Entry/Return/Call classes) to already be loaded, since bodies instantiate
// them. Runtime behaviors are bound separately via RegisterFunctionNodes.
void ImportFunctions(const std::string& jsonText, FunctionRegistry& functions,
                     NodeClassRegistry& classes, TypeRegistry& types,
                     std::vector<std::string>& errors);

// Loads a "variables" array (name + type) into the definition list.
void ImportVariables(const std::string& jsonText, std::vector<VariableDef>& variables,
                     TypeRegistry& types, std::vector<std::string>& errors);

// Loads a "comments" array into the list, assigning sequential ids.
void ImportComments(const std::string& jsonText, std::vector<Comment>& comments,
                    std::vector<std::string>& errors);

} // namespace gau
