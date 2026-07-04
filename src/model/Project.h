#pragma once

// v2 project: owns the type registry, node class registry and the active
// graph as a single save/edit unit. Non-copyable because the graph holds a
// pointer to this project's type registry.

#include "Function.h"
#include "Graph.h"
#include "NodeClassV2.h"
#include "Variable.h"

#include "core/TypeRegistry.h"

#include <memory>
#include <vector>

namespace gau {

struct Project
{
    TypeRegistry types;
    NodeClassRegistry classes;
    FunctionRegistry functions;
    std::vector<VariableDef> variables;
    std::unique_ptr<Graph> graph;

    Project() : graph(std::make_unique<Graph>(types)) {}

    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;
};

} // namespace gau
