// gau wasm build pipeline.

#include "WasmBuild.h"

#include "exec/WasmAuthoring.h"
#include "exec/WasmHost.h"
#include "exec/WasmNodes.h"
#include "platform/PlatformProcess.h"

#include <cctype>
#include <filesystem>
#include <fstream>

namespace gau {

namespace {

const char* WASM_SOURCE_DIR = "wasm_src";
const char* WASM_MODULE_DIR = "wasm";

bool IsValidFunctionName(const std::string& name)
{
    if (name.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        const bool digit = (c >= '0' && c <= '9');
        if (!(alpha || (i > 0 && digit))) {
            return false;
        }
    }
    return true;
}

// Locates a clang with the wasm32 target: a bundled toolchain beside the
// working directory first, then the default LLVM install, then PATH.
std::string FindClangForWasm()
{
    std::error_code ec;
    const char* candidates[3] = {"tools/llvm/bin/clang.exe", "tools/clang.exe",
                                 "C:/Program Files/LLVM/bin/clang.exe"};
    for (const char* candidate : candidates) {
        if (std::filesystem::exists(candidate, ec)) {
            return candidate;
        }
    }
    return "clang";
}

bool WriteTextFile(const std::string& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file << text;
    return file.good();
}

void AppendOutputLines(const std::string& output, std::vector<std::string>& log)
{
    std::string line;
    for (char c : output) {
        if (c == '\n') {
            if (!line.empty()) {
                log.push_back(line);
            }
            line.clear();
        } else if (c != '\r') {
            line += c;
        }
    }
    if (!line.empty()) {
        log.push_back(line);
    }
}

} // namespace

WasmBuildOutcome BuildWasmFunction(const std::string& name, const std::string& source,
                                   TypeRegistry& types, NodeClassRegistry& classes)
{
    WasmBuildOutcome outcome;
    if (!IsValidFunctionName(name)) {
        outcome.status = "Invalid function name (identifier expected)";
        return outcome;
    }

    std::error_code ec;
    std::filesystem::create_directories(WASM_SOURCE_DIR, ec);
    std::filesystem::create_directories(WASM_MODULE_DIR, ec);

    std::string headerError;
    if (!WriteWasmApiHeader(std::string(WASM_SOURCE_DIR) + "/gau_api.h", types, headerError)) {
        outcome.log.push_back("gau_api.h: " + headerError);
    }

    const std::string sourcePath = std::string(WASM_SOURCE_DIR) + "/" + name + ".cpp";
    if (!WriteTextFile(sourcePath, source)) {
        outcome.status = "Cannot write " + sourcePath;
        return outcome;
    }

    // Typed-signature path: pins derive from the signature and a
    // generated entry bridges the flattened host ABI.
    WasmSignature signature;
    std::string signatureError;
    const WasmSigScan scan = ScanWasmSignature(source, name, types, signature, signatureError);
    if (scan == WasmSigScan::Unsupported) {
        outcome.log.push_back("wasm: " + signatureError);
        outcome.status = "Unsupported signature (see console)";
        return outcome;
    }

    std::vector<PinDef> pins;
    std::string entryPath;
    if (scan == WasmSigScan::Found) {
        if (!BuildPinsFromWasmSignature(signature, types, pins, signatureError)) {
            outcome.log.push_back("wasm: " + signatureError);
            outcome.status = "Unsupported signature (see console)";
            return outcome;
        }
        entryPath = std::string(WASM_SOURCE_DIR) + "/" + name + ".entry.cpp";
        if (!WriteTextFile(entryPath, GenerateWasmEntrySource(signature, types))) {
            outcome.status = "Cannot write " + entryPath;
            return outcome;
        }
    }

    const std::string wasmPath = std::string(WASM_MODULE_DIR) + "/" + name + ".wasm";
    const std::string clang = FindClangForWasm();
    std::string command = "\"" + clang + "\" --target=wasm32 -nostdlib -O2"
                        + " -std=c++17 -fno-exceptions -fno-rtti"
                        + " -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined"
                        + " -o \"" + wasmPath + "\" \"" + sourcePath + "\"";
    if (!entryPath.empty()) {
        command += " \"" + entryPath + "\"";
    }

    outcome.log.push_back("--- wasm build: " + name + " ---");
    std::string output;
    int exitCode = -1;
    if (!RunCommandCaptured(command, output, exitCode)) {
        outcome.status = "Failed to launch clang: " + clang;
        outcome.log.push_back("failed to launch: " + command);
        return outcome;
    }
    AppendOutputLines(output, outcome.log);

    if (exitCode != 0) {
        if (output.find("wasm32") != std::string::npos
            && output.find("No available targets") != std::string::npos) {
            outcome.log.push_back("hint: install the official LLVM (llvm.org) or bundle a"
                                  " wasm32 clang under tools/llvm/bin");
        } else if (output.find("fatal error") != std::string::npos
                   && output.find("file not found") != std::string::npos) {
            outcome.log.push_back("hint: wasm functions build freestanding (-nostdlib);"
                                  " standard headers like <iostream>/<string> are unavailable."
                                  " Include only \"gau_api.h\" and build text with String +"
                                  " ftoa/itoa");
        }
        outcome.status = "Build failed (see console)";
        return outcome;
    }

    std::vector<std::string> loadErrors;
    WasmHost::Instance().LoadModulesFromDirectory(WASM_MODULE_DIR, loadErrors);
    for (const std::string& e : loadErrors) {
        outcome.log.push_back("wasm: " + e);
    }

    if (scan == WasmSigScan::Found) {
        const std::string category = signature.returnsVoid ? "Function" : "Pure";
        RegisterWasmNodeClass(classes, signature.functionName, category, std::move(pins),
                              "wasm:" + WasmEntryExportName(signature.functionName));
        outcome.className = signature.functionName;
        outcome.log.push_back("node class registered: " + signature.functionName);
        outcome.status = "Registered node: " + signature.functionName;
    } else {
        outcome.log.push_back("wasm build ok: no typed extern \"C\" function found;"
                              " module loaded, no class registered");
        outcome.status = "Module built (no typed function found)";
    }
    outcome.ok = true;
    return outcome;
}

} // namespace gau
