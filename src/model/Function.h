#pragma once

// v2 function definition (SRS FR-REU-1): a reusable graph exposed as a
// single callable node. A function owns its internal body graph plus an
// Entry node (marshals inputs in) and a Return node (marshals outputs
// out). exec/FunctionNodes turns a FunctionDef into Entry/Return/Call
// node classes and runtime behaviors.

#include "Graph.h"
#include "Ids.h"

#include "core/Type.h"

#include <memory>
#include <string>
#include <vector>

namespace gau {

class TypeRegistry;

struct FunctionParam
{
    std::string name;
    TypeId type = INVALID_TYPE;
};

struct FunctionDef
{
    std::string name;
    std::vector<FunctionParam> inputs;
    std::vector<FunctionParam> outputs;
    // True when the function participates in exec flow (Call node exposes an
    // exec in/out and the body runs from the Entry node). Pure functions
    // pull-evaluate the Return node instead.
    bool hasExec = false;
    // Internal graph and its marshalling endpoints.
    std::unique_ptr<Graph> body;
    NodeId entryNode = INVALID_ID;
    NodeId returnNode = INVALID_ID;

    explicit FunctionDef(const TypeRegistry& types)
        : body(std::make_unique<Graph>(types))
    {
    }
};

// Owns function definitions by stable address, keyed by name. Registering
// an existing name is disallowed here (use Find first); creation returns a
// mutable pointer so callers can build the body graph.
class FunctionRegistry
{
public:
    FunctionDef* Create(const TypeRegistry& types, std::string name);
    const FunctionDef* Find(const std::string& name) const;
    FunctionDef* Find(const std::string& name);
    const std::vector<std::unique_ptr<FunctionDef>>& All() const { return defs; }

private:
    std::vector<std::unique_ptr<FunctionDef>> defs;
};

} // namespace gau
