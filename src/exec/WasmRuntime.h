#pragma once

#include <string>
#include <vector>

class ExecContext;

// Hosts wasm3 and dispatches "wasm:<function>" node behaviors. Modules
// (*.wasm) are loaded from the wasm/ directory; exported functions take
// no parameters and talk to the node through imported host functions:
//
//   int    gau_input_i32(int inputIndex);
//   double gau_input_f64(int inputIndex);
//   int    gau_input_bool(int inputIndex);
//   int    gau_input_str(int inputIndex, char* buffer, int capacity);
//   double gau_property_f64(int propertyIndex);
//   int    gau_property_i32(int propertyIndex);
//   int    gau_property_str(int propertyIndex, char* buffer, int capacity);
//   void   gau_output_i32(int outputIndex, int value);
//   void   gau_output_f64(int outputIndex, double value);
//   void   gau_output_bool(int outputIndex, int value);
//   void   gau_output_str(int outputIndex, const char* text, int length);
//   void   gau_exec(int outputIndex);
//   void   gau_log(const char* text, int length);
class WasmRuntime
{
public:
    static WasmRuntime& Instance();

    // Replaces all loaded modules with the *.wasm files found in the
    // directory. Returns the number of modules loaded; problems are
    // appended to outErrors.
    int LoadModulesFromDirectory(const std::string& directory,
                                 std::vector<std::string>& outErrors);

    bool HasFunction(const std::string& functionName) const;

    // Runs an exported wasm function with the node context bound.
    // Returns false (with outError set) on a missing function or trap.
    bool CallNodeFunction(const std::string& functionName, ExecContext& context,
                          std::string& outError);

private:
    WasmRuntime() = default;
};
