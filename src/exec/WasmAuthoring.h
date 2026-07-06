#pragma once

// v2 wasm authoring core: everything needed to turn a typed C++ source
// into a runnable custom node, driven by the project TypeRegistry.
//
//   extern "C" String Format(const Vector3f& v)
//   { return ftoa(v.x) + ", " + ftoa(v.y); }
//
// - WriteWasmApiHeader generates wasm_src/gau_api.h: host imports, the
//   GauStr/String text helpers, and for every registry struct whose
//   leaves are all float/int/bool a C++ struct plus gau_read_<Name>/
//   gau_write_<Name> helpers over the flattened data-leaf index space
//   (matching the runtime's FlatWasmContext).
// - ScanWasmSignature finds a typed extern "C" definition; pins derive
//   from it (struct parameters stay single struct-typed pins; a void
//   return makes an exec node) and GenerateWasmEntrySource emits the
//   "<fn>__entry" bridge translation unit the class binds to.

#include "core/TypeRegistry.h"
#include "model/NodeClassV2.h"

#include <string>
#include <vector>

namespace gau {

bool WriteWasmApiHeader(const std::string& path, const TypeRegistry& types,
                        std::string& outError);

enum class WasmSigKind
{
    Int,
    Float,
    Bool,
    Str,
    Struct,
};

struct WasmSigParam
{
    WasmSigKind kind = WasmSigKind::Int;
    // Local/pin base name (parameter name, or argN when unnamed).
    std::string name;
    // C type used for the local in the generated entry ("int", "double",
    // "GauStr", or the struct name).
    std::string typeText;
    // Registry struct name when kind == Struct.
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

enum class WasmSigScan
{
    // No extern "C" definition with typed parameters or return value.
    NoTypedFunction,
    Found,
    // A typed definition exists but uses something the bridge cannot
    // marshal (pointers, unknown types, ...); outError explains.
    Unsupported,
};

// Scans (comment-stripped) source for typed extern "C" definitions.
// When several exist, one named preferredName wins, else the first.
WasmSigScan ScanWasmSignature(const std::string& source, const std::string& preferredName,
                              const TypeRegistry& types, WasmSignature& outSignature,
                              std::string& outError);

// Export name of the generated ABI bridge for a typed function.
std::string WasmEntryExportName(const std::string& functionName);

// Node pins derived from the signature: struct parameters/returns stay
// single struct-typed pins (the runtime flattens them); a void return
// makes an exec node (exec in first, exec out "then" after the data
// inputs). Non-const registry: struct pin TypeIds intern through
// UserType. Fails when a struct has non-scalar leaves.
bool BuildPinsFromWasmSignature(const WasmSignature& signature, TypeRegistry& types,
                                std::vector<PinDef>& outPins, std::string& outError);

// Companion translation unit defining WasmEntryExportName(fn): reads the
// inputs from the flattened data-leaf indices, calls the typed function
// and writes its result (continuing the exec flow for void functions).
std::string GenerateWasmEntrySource(const WasmSignature& signature, const TypeRegistry& types);

} // namespace gau
