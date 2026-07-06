#pragma once

// Builds node classes bound to wasm module exports (SRS FR-WASM, gau2
// authoring UI). The class carries execFn "wasm:<name>" so the Runtime
// dispatches its evaluation to WasmHost through the NodeEval bridge; no
// builtin behavior is registered. The export name equals the class name
// (v1 convention), so the node runs as soon as a module exporting that
// function is loaded.

#include "model/NodeClassV2.h"

#include <string>
#include <vector>

namespace gau {

NodeClass MakeWasmNodeClass(std::string name, std::string category, std::vector<PinDef> pins);

void RegisterWasmNodeClass(NodeClassRegistry& classes, std::string name, std::string category,
                           std::vector<PinDef> pins);

// Binds the class to an explicit wasm export (e.g. the generated
// "<fn>__entry" bridge of a typed signature).
void RegisterWasmNodeClass(NodeClassRegistry& classes, std::string name, std::string category,
                           std::vector<PinDef> pins, std::string execFn);

} // namespace gau
