// Make/Break node generation for struct types.

#include "StructNodes.h"

#include "core/Value.h"

#include <string>
#include <variant>

namespace gau {

void RegisterStructNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                         TypeRegistry& types, const StructDef& def)
{
    const TypeId structId = types.UserType(def.name);
    const int fieldCount = static_cast<int>(def.fields.size());

    // Make <Struct>: one input per field -> a struct output.
    NodeClass make;
    make.name = "Make " + def.name;
    make.category = "Pure";
    for (const StructField& field : def.fields) {
        make.pins.push_back(PinDef{PinDirection::Input, field.type, field.name});
    }
    make.pins.push_back(PinDef{PinDirection::Output, structId, def.name});
    classes.Register(make);

    // Break <Struct>: a struct input -> one output per field.
    NodeClass brk;
    brk.name = "Break " + def.name;
    brk.category = "Pure";
    brk.pins.push_back(PinDef{PinDirection::Input, structId, def.name});
    for (const StructField& field : def.fields) {
        brk.pins.push_back(PinDef{PinDirection::Output, field.type, field.name});
    }
    classes.Register(brk);

    builtins.Register(make.name, [fieldCount](NodeEval& e) {
        StructVal sv;
        for (int k = 0; k < fieldCount; ++k) {
            sv.fields.push_back(e.In(k));
        }
        e.Out(0, Value(ValueData(std::move(sv))));
    });

    builtins.Register(brk.name, [fieldCount](NodeEval& e) {
        const Value in = e.In(0);
        const StructVal* sv = std::get_if<StructVal>(&in.data);
        for (int k = 0; k < fieldCount; ++k) {
            const bool has = sv != nullptr && k < static_cast<int>(sv->fields.size());
            e.Out(k, has ? sv->fields[static_cast<std::size_t>(k)] : Value::None());
        }
    });
}

} // namespace gau
