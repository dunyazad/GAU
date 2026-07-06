#pragma once

// Build pipeline behind the gau function editor: source in, runnable
// custom node out. Uses the exec-layer WasmAuthoring core plus the
// platform process runner for the clang invocation.

#include "core/TypeRegistry.h"
#include "model/NodeClassV2.h"

#include <string>
#include <vector>

namespace gau {

struct WasmBuildOutcome
{
    bool ok = false;
    // One-line status for the dialog status strip.
    std::string status;
    // Full build log (compiler output, hints); echoed to the console.
    std::vector<std::string> log;
    // Registered class name (typed-signature path), empty otherwise.
    std::string className;
};

// Writes wasm_src/<name>.cpp, regenerates gau_api.h from the registry,
// scans for a typed extern "C" signature, generates the entry bridge,
// compiles everything with clang (wasm32), reloads the wasm/ modules
// and registers/updates the node class bound to the entry export.
WasmBuildOutcome BuildWasmFunction(const std::string& name, const std::string& source,
                                   TypeRegistry& types, NodeClassRegistry& classes);

} // namespace gau
