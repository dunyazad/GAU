#pragma once

#include <string>

// Runs a command line synchronously, capturing stdout+stderr. Returns
// false when the process could not be launched; the exit code and
// combined output are written otherwise. Used for the wasm build step
// (clang invocation).
bool RunCommandCaptured(const std::string& commandLine, std::string& outOutput,
                        int& outExitCode);
