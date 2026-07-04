#pragma once

// Demo node behaviors for the v2 runtime (entry, literals, arithmetic,
// print, branch). Enough to exercise data pull and exec stepping; the
// full builtin/Wasm set follows in later slices.

#include "Runtime.h"

namespace gau {

void RegisterDemoBuiltins(BuiltinRegistry& registry);

} // namespace gau
