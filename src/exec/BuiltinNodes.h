#pragma once

#include "ExecEngine.h"

// Registry of native node behaviors, keyed by exec-function name (a
// class's "execFn" JSON field, or its class name for builtins).
// Returns nullptr when unknown; the engine then uses the default
// passthrough behavior.
NodeExecFn FindNodeExecFn(const char* name);
