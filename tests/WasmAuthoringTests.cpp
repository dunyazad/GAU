#include "exec/WasmAuthoring.h"

#include "core/TypeRegistry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static TypeRegistry MakeTypes()
{
    TypeRegistry types;
    StructDef vec;
    vec.name = "Vector3f";
    vec.fields = {{"x", types.Builtin(TypeTag::Float)},
                  {"y", types.Builtin(TypeTag::Float)},
                  {"z", types.Builtin(TypeTag::Float)}};
    types.DefineStruct(vec);
    return types;
}

// The user-style formatter: struct parameter by const reference, String
// return. In v2 the struct stays a single struct-typed pin.
static void TestStructParamStringReturn()
{
    TypeRegistry types = MakeTypes();
    const std::string source =
        "#include \"gau_api.h\"\n"
        "extern \"C\" String MakeStringVector3f(const Vector3f& v)\n"
        "{\n"
        "    return ftoa(v.x) + \", \" + ftoa(v.y) + \", \" + ftoa(v.z);\n"
        "}\n";

    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "my_function", types, sig, error) == WasmSigScan::Found,
          "typed signature found");
    Check(sig.functionName == "MakeStringVector3f", "function name parsed");
    Check(sig.params.size() == 1 && sig.params[0].kind == WasmSigKind::Struct
              && sig.params[0].structName == "Vector3f",
          "struct parameter parsed");

    std::vector<PinDef> pins;
    Check(BuildPinsFromWasmSignature(sig, types, pins, error), "pins build");
    Check(pins.size() == 2, "one struct input + one string output");
    if (pins.size() == 2) {
        Check(pins[0].type == types.UserType("Vector3f") && pins[0].name == "v",
              "input stays a single Vector3f pin");
        Check(pins[1].type == types.Builtin(TypeTag::String)
                  && pins[1].direction == PinDirection::Output,
              "output pin is a string");
    }

    const std::string entry = GenerateWasmEntrySource(sig, types);
    Check(entry.find("Vector3f p0 = gau_read_Vector3f(0);") != std::string::npos,
          "entry reads the flattened struct at leaf 0");
    Check(entry.find("gau_output_str(0, result.data, result.len);") != std::string::npos,
          "entry writes the string result");
    Check(entry.find("void MakeStringVector3f__entry(void)") != std::string::npos,
          "entry export uses the __entry suffix");
}

// A void return with parameters makes an exec node whose entry continues
// the flow; the data leaf indices ignore the exec pin.
static void TestVoidReturnMakesExecNode()
{
    TypeRegistry types = MakeTypes();
    const std::string source =
        "extern \"C\" void PrintVector3f(const Vector3f& v, int count)\n"
        "{\n"
        "    gau_log(ftoa(v.x) + itoa(count));\n"
        "}\n";

    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "PrintVector3f", types, sig, error) == WasmSigScan::Found,
          "void(params) signature found");
    Check(sig.returnsVoid, "void return recognized");

    std::vector<PinDef> pins;
    Check(BuildPinsFromWasmSignature(sig, types, pins, error), "exec node pins build");
    Check(pins.size() == 4, "exec + struct + int inputs + exec out");
    if (pins.size() == 4) {
        Check(pins[0].type == types.Builtin(TypeTag::Exec)
                  && pins[0].direction == PinDirection::Input,
              "first pin is the exec input");
        Check(pins[3].type == types.Builtin(TypeTag::Exec) && pins[3].name == "then",
              "last pin is the exec output");
    }

    const std::string entry = GenerateWasmEntrySource(sig, types);
    Check(entry.find("Vector3f p0 = gau_read_Vector3f(0);") != std::string::npos,
          "struct reads at data leaf 0 despite the exec pin");
    Check(entry.find("int p1 = gau_input_i32(3);") != std::string::npos,
          "second parameter follows the struct's three leaves");
    Check(entry.find("gau_exec(0);") != std::string::npos, "entry continues the exec flow");
}

// The generated gau_api.h carries the struct with read/write helpers
// and the text helpers.
static void TestApiHeaderGeneration()
{
    TypeRegistry types = MakeTypes();
    const std::string path =
        (std::filesystem::temp_directory_path() / "gau_api_v2_test.h").string();
    std::string error;
    Check(WriteWasmApiHeader(path, types, error), "header writes");

    std::ifstream file(path, std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    Check(text.find("struct Vector3f") != std::string::npos, "struct emitted");
    Check(text.find("inline Vector3f gau_read_Vector3f(int baseIndex)") != std::string::npos,
          "read helper emitted");
    Check(text.find("inline void gau_write_Vector3f(int baseIndex") != std::string::npos,
          "write helper emitted");
    Check(text.find("typedef GauStr String;") != std::string::npos, "String alias emitted");
    Check(text.find("inline GauStr ftoa(double value") != std::string::npos, "ftoa emitted");
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// Pointers and unknown types are rejected with a message.
static void TestUnsupported()
{
    TypeRegistry types = MakeTypes();
    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature("extern \"C\" int bad(const char* t)\n{ return 0; }\n", "bad",
                            types, sig, error)
              == WasmSigScan::Unsupported,
          "pointer parameter reports unsupported");
    Check(ScanWasmSignature("extern \"C\" void f(void)\n{ }\n", "f", types, sig, error)
              == WasmSigScan::NoTypedFunction,
          "void(void) is not a typed signature");
}

int main()
{
    TestStructParamStringReturn();
    TestVoidReturnMakesExecNode();
    TestApiHeaderGeneration();
    TestUnsupported();
    if (failCount == 0) {
        std::printf("wasm_authoring_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
