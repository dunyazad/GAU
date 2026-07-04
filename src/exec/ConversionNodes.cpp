// Scalar type-conversion node behaviors and the conversion suggestion table.

#include "ConversionNodes.h"

#include "core/Value.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <variant>

namespace gau {

namespace {

std::int64_t AsInt(const Value& v)
{
    if (const std::int64_t* p = std::get_if<std::int64_t>(&v.data)) {
        return *p;
    }
    if (const double* p = std::get_if<double>(&v.data)) {
        return static_cast<std::int64_t>(*p);
    }
    if (const bool* p = std::get_if<bool>(&v.data)) {
        return *p ? 1 : 0;
    }
    return 0;
}

double AsDouble(const Value& v)
{
    if (const double* p = std::get_if<double>(&v.data)) {
        return *p;
    }
    if (const std::int64_t* p = std::get_if<std::int64_t>(&v.data)) {
        return static_cast<double>(*p);
    }
    return 0.0;
}

bool AsBool(const Value& v)
{
    if (const bool* p = std::get_if<bool>(&v.data)) {
        return *p;
    }
    if (const std::int64_t* p = std::get_if<std::int64_t>(&v.data)) {
        return *p != 0;
    }
    return false;
}

std::string AsString(const Value& v)
{
    if (const std::string* p = std::get_if<std::string>(&v.data)) {
        return *p;
    }
    return ValueToString(v);
}

// Each converter is a pure node: one typed input -> one typed output.
struct ConvDef
{
    const char* name;
    TypeTag in;
    TypeTag out;
    NodeFn fn;
};

} // namespace

void RegisterConversionNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                             TypeRegistry& types)
{
    const ConvDef defs[] = {
        {"Int To Float", TypeTag::Int, TypeTag::Float,
         [](NodeEval& e) { e.Out(0, Value::Float(AsDouble(e.In(0)))); }},
        {"Float To Int", TypeTag::Float, TypeTag::Int,
         [](NodeEval& e) { e.Out(0, Value::Int(AsInt(e.In(0)))); }},
        {"Int To String", TypeTag::Int, TypeTag::String,
         [](NodeEval& e) { e.Out(0, Value::Str(AsString(e.In(0)))); }},
        {"Float To String", TypeTag::Float, TypeTag::String,
         [](NodeEval& e) { e.Out(0, Value::Str(AsString(e.In(0)))); }},
        {"Bool To String", TypeTag::Bool, TypeTag::String,
         [](NodeEval& e) { e.Out(0, Value::Str(AsBool(e.In(0)) ? "true" : "false")); }},
        {"Int To Bool", TypeTag::Int, TypeTag::Bool,
         [](NodeEval& e) { e.Out(0, Value::Bool(AsBool(e.In(0)))); }},
        {"Bool To Int", TypeTag::Bool, TypeTag::Int,
         [](NodeEval& e) { e.Out(0, Value::Int(AsBool(e.In(0)) ? 1 : 0)); }},
        {"String To Int", TypeTag::String, TypeTag::Int,
         [](NodeEval& e) {
             e.Out(0, Value::Int(std::strtoll(AsString(e.In(0)).c_str(), nullptr, 10)));
         }},
        {"String To Float", TypeTag::String, TypeTag::Float,
         [](NodeEval& e) {
             e.Out(0, Value::Float(std::strtod(AsString(e.In(0)).c_str(), nullptr)));
         }},
    };

    for (const ConvDef& d : defs) {
        NodeClass cls;
        cls.name = d.name;
        cls.category = "Pure";
        cls.pins.push_back(PinDef{PinDirection::Input, types.Builtin(d.in), "In"});
        cls.pins.push_back(PinDef{PinDirection::Output, types.Builtin(d.out), "Out"});
        classes.Register(cls);
        builtins.Register(d.name, d.fn);
    }
}

std::string SuggestConversion(TypeTag from, TypeTag to)
{
    if (from == to) {
        return "";
    }
    struct Pair { TypeTag from; TypeTag to; const char* name; };
    static const Pair table[] = {
        {TypeTag::Int, TypeTag::Float, "Int To Float"},
        {TypeTag::Float, TypeTag::Int, "Float To Int"},
        {TypeTag::Int, TypeTag::String, "Int To String"},
        {TypeTag::Float, TypeTag::String, "Float To String"},
        {TypeTag::Bool, TypeTag::String, "Bool To String"},
        {TypeTag::Int, TypeTag::Bool, "Int To Bool"},
        {TypeTag::Bool, TypeTag::Int, "Bool To Int"},
        {TypeTag::String, TypeTag::Int, "String To Int"},
        {TypeTag::String, TypeTag::Float, "String To Float"},
    };
    for (const Pair& p : table) {
        if (p.from == from && p.to == to) {
            return p.name;
        }
    }
    return "";
}

bool InsertConversion(Graph& graph, const TypeRegistry& types, const NodeClassRegistry& classes,
                      PinId a, PinId b)
{
    const Pin* pa = graph.FindPin(a);
    const Pin* pb = graph.FindPin(b);
    if (pa == nullptr || pb == nullptr || pa->direction == pb->direction) {
        return false;
    }
    // Orient into (output -> input).
    const Pin* outPin = (pa->direction == PinDirection::Output) ? pa : pb;
    const Pin* inPin = (pa->direction == PinDirection::Output) ? pb : pa;
    if (outPin->node == inPin->node || outPin->type == inPin->type) {
        return false;
    }

    const TypeDesc* outDesc = types.Resolve(outPin->type);
    const TypeDesc* inDesc = types.Resolve(inPin->type);
    if (outDesc == nullptr || inDesc == nullptr) {
        return false;
    }
    const std::string convName = SuggestConversion(outDesc->tag, inDesc->tag);
    if (convName.empty()) {
        return false;
    }
    const NodeClass* cls = classes.Find(convName);
    if (cls == nullptr) {
        return false;
    }

    const Node* outNode = graph.FindPinOwner(outPin->id);
    const Node* inNode = graph.FindPinOwner(inPin->id);
    const float x = (outNode != nullptr && inNode != nullptr) ? (outNode->x + inNode->x) * 0.5f
                                                              : 0.0f;
    const float y = (outNode != nullptr && inNode != nullptr) ? (outNode->y + inNode->y) * 0.5f
                                                              : 0.0f;
    const PinId sourcePin = outPin->id;
    const PinId sinkPin = inPin->id;
    const NodeId conv = graph.AddNode(*cls, x, y);
    const Node* convNode = graph.FindNode(conv);
    if (convNode == nullptr || convNode->inputs.empty() || convNode->outputs.empty()) {
        return false;
    }
    graph.AddLink(sourcePin, convNode->inputs[0].id);
    graph.AddLink(convNode->outputs[0].id, sinkPin);
    return true;
}

} // namespace gau
