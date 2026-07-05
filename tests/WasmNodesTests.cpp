#include "exec/WasmNodes.h"
#include "io/ProjectExport.h"
#include "io/ProjectFile.h"
#include "model/Project.h"

#include "core/TypeRegistry.h"

#include <cstdio>
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

// A wasm node class built by the authoring helper carries the wasm: exec
// binding and the exact pin list handed in, and re-registering the same
// name replaces the previous definition.
static void TestRegisterWasmNodeClass()
{
    TypeRegistry types;
    NodeClassRegistry classes;
    const TypeId i = types.Builtin(TypeTag::Int);
    const TypeId f = types.Builtin(TypeTag::Float);

    RegisterWasmNodeClass(classes, "AddInts", "CustomObject",
                          {{PinDirection::Input, i, "A"},
                           {PinDirection::Input, i, "B"},
                           {PinDirection::Output, i, "Sum"}});

    const NodeClass* cls = classes.Find("AddInts");
    Check(cls != nullptr, "class registered under its name");
    if (cls != nullptr) {
        Check(cls->execFn == "wasm:AddInts", "execFn is wasm:<class name>");
        Check(cls->category == "CustomObject", "category preserved");
        Check(cls->pins.size() == 3, "pin list preserved");
        Check(cls->pins[2].direction == PinDirection::Output && cls->pins[2].name == "Sum",
              "output pin preserved in order");
    }

    RegisterWasmNodeClass(classes, "AddInts", "Pure", {{PinDirection::Output, f, "R"}});
    cls = classes.Find("AddInts");
    Check(cls != nullptr && cls->pins.size() == 1 && cls->category == "Pure",
          "re-registering the same name replaces the class");
}

// An authored wasm class survives a project save/load round trip: the
// exported JSON reproduces the class with its execFn binding intact.
static void TestWasmClassRoundTrip()
{
    Project project;
    const TypeId i = project.types.Builtin(TypeTag::Int);
    RegisterWasmNodeClass(project.classes, "Doubler", "CustomObject",
                          {{PinDirection::Input, i, "In"}, {PinDirection::Output, i, "Out"}});

    const std::string text = ExportProject(project);

    Project loaded;
    std::vector<std::string> errors;
    LoadProjectText(text, loaded, errors);
    Check(errors.empty(), "round trip loads without errors");

    const NodeClass* cls = loaded.classes.Find("Doubler");
    Check(cls != nullptr, "wasm class survives save/load");
    if (cls != nullptr) {
        Check(cls->execFn == "wasm:Doubler", "execFn survives save/load");
        Check(cls->pins.size() == 2, "pins survive save/load");
    }
}

int main()
{
    TestRegisterWasmNodeClass();
    TestWasmClassRoundTrip();
    if (failCount == 0) {
        std::printf("wasm_nodes_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
