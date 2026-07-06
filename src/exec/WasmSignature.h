#pragma once

// Typed wasm function signatures: instead of the index-based host ABI
// (void fn(void) + gau_input_*/gau_output_* + @in/@out directives), a
// user may write a natural extern "C" function such as
//
//   extern "C" String Format(const Vector3f& v)
//   { return ftoa(v.x) + ", " + ftoa(v.y); }
//
// The build scans the source for such a definition, derives the node
// pins from the parameter/return types and generates a companion entry
// translation unit ("<name>__entry") that bridges the host ABI to the
// typed call. Supported value types: int, long, float, double, bool,
// String (GauStr) and any data-carrier class from gau_api.h (flattened
// to one scalar pin per field).

#include "model/NodeClass.h"

#include <string>
#include <vector>

enum class WasmValueKind
{
    Int,
    Float,
    Bool,
    Str,
    Struct,
};

struct WasmSigParam
{
    WasmValueKind kind = WasmValueKind::Int;
    // Local/pin base name (parameter name, or argN when unnamed).
    std::string name;
    // C type used for the local in the generated entry ("int", "double",
    // "GauStr", or the struct name).
    std::string typeText;
    // Data-carrier class name when kind == Struct.
    std::string structName;
};

struct WasmSignature
{
    std::string functionName;
    // Verbatim (trimmed) texts reused in the entry's declaration so the
    // declared type matches the user's definition exactly.
    std::string returnTypeText;
    std::string paramListText;
    bool returnsVoid = true;
    WasmSigParam returnValue;
    std::vector<WasmSigParam> params;
};

enum class WasmSignatureScan
{
    // No extern "C" definition with typed parameters or return value;
    // callers fall back to the directive/manual path.
    NoTypedFunction,
    Found,
    // A typed definition exists but uses something the bridge cannot
    // marshal (pointers, unknown types, ...); outError explains.
    Unsupported,
};

// Scans (comment-stripped) source for typed extern "C" definitions.
// When several exist, one named preferredName wins, else the first.
WasmSignatureScan ScanWasmSignature(const std::string& source, const std::string& preferredName,
                                    WasmSignature& outSignature, std::string& outError);

// Export name of the generated ABI bridge for a typed function.
std::string WasmEntryExportName(const std::string& functionName);

// Node pins derived from the signature: one pin per scalar parameter,
// field-flattened pins for struct parameters, then the output pin(s).
// Fails when a struct type is not a data-carrier class.
bool BuildPinsFromWasmSignature(const WasmSignature& signature, std::vector<PinDef>& outPins,
                                std::string& outError);

// Companion translation unit defining WasmEntryExportName(fn): reads the
// inputs, calls the typed function and writes its result to the outputs.
std::string GenerateWasmEntrySource(const WasmSignature& signature);
