#pragma once

// Hosts wasm3 for v2 custom-node execution (SRS FR-WASM-1..3). Modules
// (*.wasm) load from a directory; each exported function takes no parameters
// and talks to its node through imported host functions
// (gau_input_*/gau_property_*/gau_output_*/gau_exec/gau_log). The host ABI is
// identical to the v1 runtime, so existing .wasm modules run unchanged, but
// this port speaks the v2 core Value world so the exec layer stays free of v1
// types.

#include "core/Value.h"

#include <string>
#include <vector>

namespace gau {

// The node a wasm export reads inputs/properties from and writes outputs to.
// The runtime implements this over a NodeEval; indices match pin order.
class WasmNodeContext
{
public:
    virtual ~WasmNodeContext() = default;
    virtual Value Input(int index) = 0;
    virtual Value Property(int index) = 0;
    virtual void SetOutput(int index, Value value) = 0;
    virtual void RunExec(int index) = 0;
    virtual void Log(const std::string& message) = 0;
};

class WasmHost
{
public:
    static WasmHost& Instance();

    // Replaces all loaded modules with the *.wasm files in the directory.
    // Returns the count loaded; problems are appended to outErrors.
    int LoadModulesFromDirectory(const std::string& directory,
                                 std::vector<std::string>& outErrors);

    bool HasFunction(const std::string& functionName) const;

    // Runs an exported function with the node context bound. Returns false
    // (outError set) on a missing function or a trap.
    bool CallNodeFunction(const std::string& functionName, WasmNodeContext& context,
                          std::string& outError);

private:
    WasmHost() = default;
};

} // namespace gau
