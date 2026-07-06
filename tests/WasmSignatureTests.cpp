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
    TestVoidFunctionIsNotTyped();
    TestPointerUnsupported();
    TestCommentIgnored();
    if (failCount == 0) {
        std::printf("wasm_signature_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
