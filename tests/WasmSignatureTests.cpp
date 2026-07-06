#include "exec/WasmSignature.h"
#include "model/NodeClass.h"

#include <cstdio>
#include <string>
#include <vector>

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

// Vector3f data-carrier class, mirroring the one users author in-app.
static const NodeClass Vector3fClass(
    "Vector3f", "Object", {},
    {{"x", PropertyContainer::None, PinType::Float, PinType::String, Value(0.0), {}, {}, "", ""},
     {"y", PropertyContainer::None, PinType::Float, PinType::String, Value(0.0), {}, {}, "", ""},
     {"z", PropertyContainer::None, PinType::Float, PinType::String, Value(0.0), {}, {}, "", ""}});

// The user-style formatter: struct parameter by const reference, String
// return, no directives. Pins and the generated entry must derive fully
// from the signature.
static void TestStructParamStringReturn()
{
    const std::string source =
        "#include \"gau_api.h\"\n"
        "extern \"C\" String MakeStringVector3f(const Vector3f& v)\n"
        "{\n"
        "    return ftoa(v.x) + \", \" + ftoa(v.y) + \", \" + ftoa(v.z);\n"
        "}\n";

    WasmSignature sig;
    std::string error;
    const WasmSignatureScan scan = ScanWasmSignature(source, "my_function", sig, error);
    Check(scan == WasmSignatureScan::Found, "typed signature found");
    if (scan != WasmSignatureScan::Found) {
        return;
    }
    Check(sig.functionName == "MakeStringVector3f", "function name parsed");
    Check(sig.params.size() == 1 && sig.params[0].kind == WasmValueKind::Struct
              && sig.params[0].structName == "Vector3f" && sig.params[0].name == "v",
          "struct parameter parsed");
    Check(!sig.returnsVoid && sig.returnValue.kind == WasmValueKind::Str,
          "String return parsed");

    std::vector<PinDef> pins;
    Check(BuildPinsFromWasmSignature(sig, pins, error), "pins build");
    Check(pins.size() == 4, "3 flattened inputs + 1 output");
    if (pins.size() == 4) {
        Check(pins[0].direction == PinDirection::Input && pins[0].type == PinType::Float
                  && pins[0].name == "v_x",
              "first input pin is v_x float");
        Check(pins[3].direction == PinDirection::Output && pins[3].type == PinType::String,
              "output pin is a string");
    }

    const std::string entry = GenerateWasmEntrySource(sig);
    Check(entry.find("Vector3f p0 = gau_read_Vector3f(0);") != std::string::npos,
          "entry reads the flattened struct");
    Check(entry.find("MakeStringVector3f(p0)") != std::string::npos,
          "entry calls the user function");
    Check(entry.find("gau_output_str(0, result.data, result.len);") != std::string::npos,
          "entry writes the string result");
    Check(entry.find("void MakeStringVector3f__entry(void)") != std::string::npos,
          "entry export uses the __entry suffix");
    Check(entry.find("extern \"C\" String MakeStringVector3f(const Vector3f& v);")
              != std::string::npos,
          "entry declares the user function verbatim");
}

// Scalar parameters and an int return; unnamed parameters get argN.
static void TestScalarSignature()
{
    const std::string source = "extern \"C\" int add2(int a, int)\n{ return 0; }\n";
    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "add2", sig, error) == WasmSignatureScan::Found,
          "scalar signature found");
    std::vector<PinDef> pins;
    Check(BuildPinsFromWasmSignature(sig, pins, error), "scalar pins build");
    Check(pins.size() == 3 && pins[0].name == "a" && pins[1].name == "arg1"
              && pins[2].direction == PinDirection::Output && pins[2].type == PinType::Int,
          "a + arg1 inputs and int result output");
}

// A void return with parameters makes an exec node: exec in first, data
// inputs from index 1, exec out last, and the entry continues the flow.
static void TestVoidReturnMakesExecNode()
{
    const std::string source =
        "#include \"gau_api.h\"\n"
        "extern \"C\" void PrintVector3f(const Vector3f& v)\n"
        "{\n"
        "    gau_log(ftoa(v.x));\n"
        "}\n";

    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "PrintVector3f", sig, error) == WasmSignatureScan::Found,
          "void(param) signature found");
    Check(sig.returnsVoid, "void return recognized");

    std::vector<PinDef> pins;
    Check(BuildPinsFromWasmSignature(sig, pins, error), "exec node pins build");
    Check(pins.size() == 5, "exec + 3 data inputs + exec out");
    if (pins.size() == 5) {
        Check(pins[0].direction == PinDirection::Input && pins[0].type == PinType::Exec,
              "first pin is the exec input");
        Check(pins[1].name == "v_x" && pins[1].type == PinType::Float,
              "data inputs follow the exec pin");
        Check(pins[4].direction == PinDirection::Output && pins[4].type == PinType::Exec
                  && pins[4].name == "then",
              "last pin is the exec output");
    }

    const std::string entry = GenerateWasmEntrySource(sig);
    Check(entry.find("Vector3f p0 = gau_read_Vector3f(0);") != std::string::npos,
          "entry reads data inputs starting at 0 (data-only index space)");
    Check(entry.find("gau_exec(0);") != std::string::npos,
          "entry continues the exec flow");
}

// The classic manual-ABI form stays on the directive path.
static void TestVoidFunctionIsNotTyped()
{
    const std::string source =
        "// @in a:int b:int\n"
        "extern \"C\" void my_function(void)\n"
        "{\n    gau_output_i32(0, gau_input_i32(0) + gau_input_i32(1));\n}\n";
    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "my_function", sig, error)
              == WasmSignatureScan::NoTypedFunction,
          "void(void) is not a typed signature");
}

// Pointers cannot be marshalled; the scan must say so instead of
// building a broken entry.
static void TestPointerUnsupported()
{
    const std::string source = "extern \"C\" int bad(const char* text)\n{ return 0; }\n";
    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "bad", sig, error) == WasmSignatureScan::Unsupported,
          "pointer parameter reports unsupported");
    Check(!error.empty(), "unsupported error has a message");
}

// Commented-out functions are invisible to the scan.
static void TestCommentIgnored()
{
    const std::string source =
        "// extern \"C\" int ghost(int a) { return a; }\n"
        "/* extern \"C\" int ghost2(int a) { return a; } */\n";
    WasmSignature sig;
    std::string error;
    Check(ScanWasmSignature(source, "ghost", sig, error) == WasmSignatureScan::NoTypedFunction,
          "commented functions are ignored");
}

int main()
{
    TestStructParamStringReturn();
    TestScalarSignature();
    TestVoidReturnMakesExecNode();
    TestVoidFunctionIsNotTyped();
    TestPointerUnsupported();
    TestCommentIgnored();
    if (failCount == 0) {
        std::printf("wasm_signature_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
