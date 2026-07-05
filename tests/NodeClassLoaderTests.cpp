#include "model/NodeClass.h"
#include "model/NodeClassLoader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
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

static std::string WriteTempClassFile(const char* fileName, const char* text)
{
    const std::string path =
        (std::filesystem::temp_directory_path() / fileName).string();
    std::ofstream file(path, std::ios::binary);
    file << text;
    return path;
}

// An Object-category node class is usable as a nominal pin/field type
// (FR-TYP-6) when loading, including a forward reference: the consumer
// entry appears BEFORE the Object class in the file (delete/re-add cycles
// reorder entries), and a struct field may also use the class name.
static void TestObjectClassPinType()
{
    const char* text = R"json({
      "nodeClasses": [
        {
          "category": "Function",
          "name": "FormatVec3",
          "pins": [
            {"direction": "in", "name": "v", "type": "Vec3"},
            {"direction": "out", "name": "text", "type": "string"}
          ]
        },
        {
          "category": "Object",
          "name": "Vec3",
          "pins": [
            {"direction": "in", "name": "x", "type": "float"},
            {"direction": "in", "name": "y", "type": "float"},
            {"direction": "in", "name": "z", "type": "float"}
          ]
        }
      ],
      "types": [
        {
          "kind": "struct",
          "name": "Ray",
          "fields": [
            {"name": "origin", "type": "Vec3"},
            {"name": "length", "type": "float"}
          ]
        }
      ]
    })json";

    const std::string path = WriteTempClassFile("gau_loader_objtype.json", text);
    std::vector<std::string> errors;
    const int loaded = LoadNodeClassesFromFile(path, errors);
    for (const std::string& e : errors) {
        std::printf("  [load] %s\n", e.c_str());
    }
    Check(errors.empty(), "no load errors");
    Check(loaded == 2, "both classes load");

    const NodeClass* consumer = NodeClass::FindByName("FormatVec3");
    Check(consumer != nullptr, "consumer class registered");
    if (consumer != nullptr) {
        Check(consumer->GetPins().size() == 2, "consumer keeps both pins");
        Check(consumer->GetPins()[0].type == PinType::UserType
                  && consumer->GetPins()[0].typeName == "Vec3",
              "object class name resolves to a UserType pin (forward reference)");
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// A pin type naming nothing (no builtin, no user type, no Object class)
// is still rejected with an error, and the class is skipped.
static void TestUnknownTypeStillRejected()
{
    const char* text = R"json({
      "nodeClasses": [
        {
          "category": "Function",
          "name": "BadPinClass",
          "pins": [
            {"direction": "in", "name": "v", "type": "NoSuchType"}
          ]
        }
      ]
    })json";

    const std::string path = WriteTempClassFile("gau_loader_badtype.json", text);
    std::vector<std::string> errors;
    const int loaded = LoadNodeClassesFromFile(path, errors);
    Check(loaded == 0, "class with unknown pin type is skipped");
    Check(!errors.empty(), "unknown pin type reports an error");
    Check(NodeClass::FindByName("BadPinClass") == nullptr, "bad class not registered");
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

int main()
{
    TestObjectClassPinType();
    TestUnknownTypeStillRejected();
    if (failCount == 0) {
        std::printf("node_class_loader_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
