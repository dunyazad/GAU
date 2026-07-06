#include "WasmRuntime.h"
#include "ExecEngine.h"

#include <wasm3.h>
#include <m3_env.h>

#include <cstring>
#include <filesystem>
#include <fstream>

// wasm3 handles and the context bound during a call. Single-threaded
// (main loop only), so one active context is enough. Module byte
// buffers must outlive their modules: wasm3 references them in place.
static IM3Environment g_environment = nullptr;
static IM3Runtime g_runtime = nullptr;
static ExecContext* g_activeContext = nullptr;
static std::vector<std::vector<char>> g_moduleBytes;

static int CoerceInt(const Value& value)
{
    if (const int* intValue = std::get_if<int>(&value)) {
        return *intValue;
    }
    if (const double* doubleValue = std::get_if<double>(&value)) {
        return static_cast<int>(*doubleValue);
    }
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        return *boolValue ? 1 : 0;
    }
    return 0;
}

static double CoerceDouble(const Value& value)
{
    if (const double* doubleValue = std::get_if<double>(&value)) {
        return *doubleValue;
    }
    if (const int* intValue = std::get_if<int>(&value)) {
        return static_cast<double>(*intValue);
    }
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        return *boolValue ? 1.0 : 0.0;
    }
    return 0.0;
}

static std::string CoerceString(const Value& value)
{
    if (const std::string* stringValue = std::get_if<std::string>(&value)) {
        return *stringValue;
    }
    return ValueToString(value);
}

// Copies text into wasm linear memory (truncating to capacity) and
// returns the copied length.
static int CopyToWasmBuffer(const std::string& text, char* buffer, int capacity)
{
    if (buffer == nullptr || capacity <= 0) {
        return 0;
    }
    int length = static_cast<int>(text.size());
    if (length > capacity) {
        length = capacity;
    }
    std::memcpy(buffer, text.data(), static_cast<std::size_t>(length));
    return length;
}

m3ApiRawFunction(GauInputI32)
{
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, index);
    m3ApiReturn(g_activeContext ? CoerceInt(g_activeContext->GetDataInput(index)) : 0);
}

m3ApiRawFunction(GauInputF64)
{
    m3ApiReturnType(double);
    m3ApiGetArg(int32_t, index);
    m3ApiReturn(g_activeContext ? CoerceDouble(g_activeContext->GetDataInput(index)) : 0.0);
}

m3ApiRawFunction(GauInputBool)
{
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, index);
    int32_t result = 0;
    if (g_activeContext != nullptr) {
        result = CoerceInt(g_activeContext->GetDataInput(index)) != 0 ? 1 : 0;
    }
    m3ApiReturn(result);
}

m3ApiRawFunction(GauInputStr)
{
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, index);
    m3ApiGetArgMem(char*, buffer);
    m3ApiGetArg(int32_t, capacity);
    int32_t length = 0;
    if (g_activeContext != nullptr) {
        length = CopyToWasmBuffer(CoerceString(g_activeContext->GetDataInput(index)),
                                  buffer, capacity);
    }
    m3ApiReturn(length);
}

m3ApiRawFunction(GauPropertyI32)
{
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, index);
    m3ApiReturn(g_activeContext ? CoerceInt(g_activeContext->GetProperty(index)) : 0);
}

m3ApiRawFunction(GauPropertyF64)
{
    m3ApiReturnType(double);
    m3ApiGetArg(int32_t, index);
    m3ApiReturn(g_activeContext ? CoerceDouble(g_activeContext->GetProperty(index)) : 0.0);
}

m3ApiRawFunction(GauPropertyStr)
{
    m3ApiReturnType(int32_t);
    m3ApiGetArg(int32_t, index);
    m3ApiGetArgMem(char*, buffer);
    m3ApiGetArg(int32_t, capacity);
    int32_t length = 0;
    if (g_activeContext != nullptr) {
        length = CopyToWasmBuffer(CoerceString(g_activeContext->GetProperty(index)),
                                  buffer, capacity);
    }
    m3ApiReturn(length);
}

m3ApiRawFunction(GauOutputI32)
{
    m3ApiGetArg(int32_t, index);
    m3ApiGetArg(int32_t, value);
    if (g_activeContext != nullptr) {
        g_activeContext->SetDataOutput(index, Value(static_cast<int>(value)));
    }
    m3ApiSuccess();
}

m3ApiRawFunction(GauOutputF64)
{
    m3ApiGetArg(int32_t, index);
    m3ApiGetArg(double, value);
    if (g_activeContext != nullptr) {
        g_activeContext->SetDataOutput(index, Value(value));
    }
    m3ApiSuccess();
}

m3ApiRawFunction(GauOutputBool)
{
    m3ApiGetArg(int32_t, index);
    m3ApiGetArg(int32_t, value);
    if (g_activeContext != nullptr) {
        g_activeContext->SetDataOutput(index, Value(value != 0));
    }
    m3ApiSuccess();
}

m3ApiRawFunction(GauOutputStr)
{
    m3ApiGetArg(int32_t, index);
    m3ApiGetArgMem(const char*, text);
    m3ApiGetArg(int32_t, length);
    if (g_activeContext != nullptr && text != nullptr && length >= 0) {
        g_activeContext->SetDataOutput(index,
                                   Value(std::string(text, static_cast<std::size_t>(length))));
    }
    m3ApiSuccess();
}

m3ApiRawFunction(GauExec)
{
    m3ApiGetArg(int32_t, index);
    if (g_activeContext != nullptr) {
        g_activeContext->RunExecFlow(index);
    }
    m3ApiSuccess();
}

m3ApiRawFunction(GauLog)
{
    m3ApiGetArgMem(const char*, text);
    m3ApiGetArg(int32_t, length);
    if (g_activeContext != nullptr && text != nullptr && length >= 0) {
        g_activeContext->Log(std::string(text, static_cast<std::size_t>(length)));
    }
    m3ApiSuccess();
}

// Linking a host function into a module that does not import it fails
// with a lookup error; that is expected and ignored.
static void LinkHostFunction(IM3Module module, const char* name, const char* signature,
                             M3RawCall function)
{
    const M3Result result = m3_LinkRawFunction(module, "env", name, signature, function);
    (void)result;
}

static void LinkHostFunctions(IM3Module module)
{
    LinkHostFunction(module, "gau_input_i32", "i(i)", GauInputI32);
    LinkHostFunction(module, "gau_input_f64", "F(i)", GauInputF64);
    LinkHostFunction(module, "gau_input_bool", "i(i)", GauInputBool);
    LinkHostFunction(module, "gau_input_str", "i(iii)", GauInputStr);
    LinkHostFunction(module, "gau_property_i32", "i(i)", GauPropertyI32);
    LinkHostFunction(module, "gau_property_f64", "F(i)", GauPropertyF64);
    LinkHostFunction(module, "gau_property_str", "i(iii)", GauPropertyStr);
    LinkHostFunction(module, "gau_output_i32", "v(ii)", GauOutputI32);
    LinkHostFunction(module, "gau_output_f64", "v(iF)", GauOutputF64);
    LinkHostFunction(module, "gau_output_bool", "v(ii)", GauOutputBool);
    LinkHostFunction(module, "gau_output_str", "v(iii)", GauOutputStr);
    LinkHostFunction(module, "gau_exec", "v(i)", GauExec);
    LinkHostFunction(module, "gau_log", "v(ii)", GauLog);
}

WasmRuntime& WasmRuntime::Instance()
{
    static WasmRuntime instance;
    return instance;
}

static bool LoadOneModule(const std::string& path, std::string& outError)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        outError = "cannot open module: " + path;
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        outError = "empty module: " + path;
        return false;
    }
    // wasm3 references the binary in place; keep it alive with the
    // runtime.
    g_moduleBytes.push_back(std::move(bytes));
    const std::vector<char>& storedBytes = g_moduleBytes.back();

    IM3Module module = nullptr;
    M3Result result = m3_ParseModule(g_environment, &module,
                                     reinterpret_cast<const uint8_t*>(storedBytes.data()),
                                     static_cast<uint32_t>(storedBytes.size()));
    if (result != m3Err_none) {
        outError = std::string("parse failed: ") + result + ": " + path;
        return false;
    }
    // The runtime owns the module after a successful load.
    result = m3_LoadModule(g_runtime, module);
    if (result != m3Err_none) {
        m3_FreeModule(module);
        outError = std::string("load failed: ") + result + ": " + path;
        return false;
    }
    LinkHostFunctions(module);
    return true;
}

int WasmRuntime::LoadModulesFromDirectory(const std::string& directory,
                                          std::vector<std::string>& outErrors)
{
    if (g_runtime != nullptr) {
        m3_FreeRuntime(g_runtime);
        g_runtime = nullptr;
    }
    g_moduleBytes.clear();
    if (g_environment == nullptr) {
        g_environment = m3_NewEnvironment();
    }
    g_runtime = m3_NewRuntime(g_environment, 64 * 1024, nullptr);
    if (g_runtime == nullptr) {
        outErrors.push_back("wasm runtime creation failed");
        return 0;
    }

    int loadedCount = 0;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        return 0;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (entry.path().extension() != ".wasm") {
            continue;
        }
        std::string error;
        if (LoadOneModule(entry.path().string(), error)) {
            ++loadedCount;
        } else {
            outErrors.push_back(error);
        }
    }
    return loadedCount;
}

bool WasmRuntime::HasFunction(const std::string& functionName) const
{
    if (g_runtime == nullptr) {
        return false;
    }
    IM3Function function = nullptr;
    return m3_FindFunction(&function, g_runtime, functionName.c_str()) == m3Err_none;
}

bool WasmRuntime::CallNodeFunction(const std::string& functionName, ExecContext& context,
                                   std::string& outError)
{
    if (g_runtime == nullptr) {
        outError = "wasm runtime not initialized";
        return false;
    }
    IM3Function function = nullptr;
    M3Result result = m3_FindFunction(&function, g_runtime, functionName.c_str());
    if (result != m3Err_none) {
        outError = "wasm function not found: " + functionName;
        return false;
    }

    ExecContext* previousContext = g_activeContext;
    g_activeContext = &context;
    result = m3_CallV(function);
    g_activeContext = previousContext;

    if (result != m3Err_none) {
        outError = std::string("wasm trap in ") + functionName + ": " + result;
        return false;
    }
    return true;
}
