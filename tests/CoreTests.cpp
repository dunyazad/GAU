#include "core/TypeRegistry.h"
#include "core/Value.h"

#include <cstdio>
#include <string>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static void CheckStr(const std::string& actual, const std::string& expected, const char* label)
{
    if (actual != expected) {
        std::printf("FAIL: %s: expected '%s', got '%s'\n", label, expected.c_str(),
                    actual.c_str());
        ++failCount;
    }
}

static void TestBuiltinsAndInterning()
{
    TypeRegistry reg;
    const TypeId intId = reg.Builtin(TypeTag::Int);
    Check(intId != INVALID_TYPE, "int builtin valid");
    Check(reg.Builtin(TypeTag::Int) == intId, "builtin stable");

    const TypeId a = reg.ArrayOf(intId);
    const TypeId b = reg.ArrayOf(intId);
    Check(a == b, "array interning dedups");
    Check(reg.ArrayOf(reg.Builtin(TypeTag::Float)) != a, "distinct arrays differ");
    CheckStr(reg.TypeName(a), "Array<int>", "array type name");

    const TypeId m = reg.MapOf(reg.Builtin(TypeTag::String), intId);
    CheckStr(reg.TypeName(m), "Map<string, int>", "map type name");
}

static void TestUserTypes()
{
    TypeRegistry reg;
    EnumDef dir;
    dir.name = "Direction";
    dir.values = {"North", "South"};
    reg.DefineEnum(dir);

    StructDef vec;
    vec.name = "Vector3f";
    vec.fields = {{"x", reg.Builtin(TypeTag::Float)},
                  {"y", reg.Builtin(TypeTag::Float)},
                  {"z", reg.Builtin(TypeTag::Float)}};
    reg.DefineStruct(vec);

    const TypeId dirId = reg.UserType("Direction");
    const TypeId vecId = reg.UserType("Vector3f");
    Check(reg.Resolve(dirId)->tag == TypeTag::Enum, "Direction resolves as enum");
    Check(reg.Resolve(vecId)->tag == TypeTag::Struct, "Vector3f resolves as struct");
    Check(reg.UserType("Unknown") != INVALID_TYPE, "unknown user type -> object alias");
    Check(reg.Resolve(reg.UserType("Unknown"))->tag == TypeTag::Object, "unknown -> object");

    CheckStr(reg.TypeName(reg.ArrayOf(vecId)), "Array<Vector3f>", "nested container name");
}

static void TestDefaults()
{
    TypeRegistry reg;
    StructDef vec;
    vec.name = "Vector3f";
    vec.fields = {{"x", reg.Builtin(TypeTag::Float)}, {"y", reg.Builtin(TypeTag::Float)}};
    reg.DefineStruct(vec);
    const TypeId vecId = reg.UserType("Vector3f");

    const Value def = reg.MakeDefault(vecId);
    const StructVal* sv = std::get_if<StructVal>(&def.data);
    Check(sv != nullptr && sv->fields.size() == 2, "struct default has 2 fields");
    Check(sv != nullptr && std::get<double>(sv->fields[0].data) == 0.0, "field default 0.0");

    const Value arrDef = reg.MakeDefault(reg.ArrayOf(reg.Builtin(TypeTag::Int)));
    const ArrayVal* av = std::get_if<ArrayVal>(&arrDef.data);
    Check(av != nullptr && av->items.empty(), "array default empty");

    Check(std::get<std::int64_t>(reg.MakeDefault(reg.Builtin(TypeTag::Int)).data) == 0,
          "int default 0");
}

static void TestValueEqualityAndString()
{
    Check(Value::Int(5) == Value::Int(5), "int equality");
    Check(Value::Int(5) != Value::Int(6), "int inequality");
    Check(Value::Bool(true) != Value::Int(1), "type-distinct inequality");

    StructVal s;
    s.fields = {Value::Float(1.0), Value::Float(2.0)};
    StructVal s2 = s;
    Check(Value(ValueData(s)) == Value(ValueData(s2)), "struct equality");

    ArrayVal a;
    a.items = {Value::Int(1), Value::Int(2), Value::Int(3)};
    CheckStr(ValueToString(Value(ValueData(a))), "[1, 2, 3]", "array to string");
    CheckStr(ValueToString(Value(ValueData(s))), "{1, 2}", "struct to string");
    CheckStr(ValueToString(Value::Str("hi")), "hi", "string to string");
    CheckStr(ValueToString(Value::Bool(false)), "false", "bool to string");
}

int main()
{
    TestBuiltinsAndInterning();
    TestUserTypes();
    TestDefaults();
    TestValueEqualityAndString();
    if (failCount == 0) {
        std::printf("core_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
