#include "core/TypeRegistry.h"
#include "exec/ConversionNodes.h"
#include "exec/Runtime.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include <cstdio>
#include <string>
#include <variant>
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

// Registers a maker class per scalar that emits a fixed property value, so a
// converter can pull a typed source.
static void RegisterMakers(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                           const TypeRegistry& t)
{
    const TypeId i = t.Builtin(TypeTag::Int);
    const TypeId f = t.Builtin(TypeTag::Float);
    const TypeId b = t.Builtin(TypeTag::Bool);
    const TypeId s = t.Builtin(TypeTag::String);
    NodeClass mi;
    mi.name = "MI";
    mi.category = "Pure";
    mi.pins.push_back(PinDef{PinDirection::Output, i, "V"});
    mi.properties.push_back(PropertyDef{"V", i, Value::Int(0)});
    classes.Register(mi);
    NodeClass mf = mi;
    mf.name = "MF";
    mf.pins[0].type = f;
    mf.properties[0].type = f;
    mf.properties[0].defaultValue = Value::Float(0.0);
    classes.Register(mf);
    NodeClass mb = mi;
    mb.name = "MB";
    mb.pins[0].type = b;
    mb.properties[0].type = b;
    mb.properties[0].defaultValue = Value::Bool(false);
    classes.Register(mb);
    NodeClass ms = mi;
    ms.name = "MS";
    ms.pins[0].type = s;
    ms.properties[0].type = s;
    ms.properties[0].defaultValue = Value::Str("");
    classes.Register(ms);
    builtins.Register("MI", [](NodeEval& e) { e.Out(0, e.Prop(0)); });
    builtins.Register("MF", [](NodeEval& e) { e.Out(0, e.Prop(0)); });
    builtins.Register("MB", [](NodeEval& e) { e.Out(0, e.Prop(0)); });
    builtins.Register("MS", [](NodeEval& e) { e.Out(0, e.Prop(0)); });
}

static Value Convert(const char* maker, Value in, const char* conv)
{
    TypeRegistry t;
    NodeClassRegistry classes;
    BuiltinRegistry builtins;
    RegisterMakers(classes, builtins, t);
    RegisterConversionNodes(classes, builtins, t);

    Graph g(t);
    const NodeId src = g.AddNode(*classes.Find(maker), 0, 0);
    const NodeId cvt = g.AddNode(*classes.Find(conv), 0, 0);
    g.FindNode(src)->properties[0] = std::move(in);
    g.AddLink(g.FindNode(src)->outputs[0].id, g.FindNode(cvt)->inputs[0].id);

    Runtime rt(g, t, classes, builtins, nullptr);
    rt.Start(INVALID_ID); // no exec; use pull evaluation
    return rt.EvalPin(g.FindNode(cvt)->outputs[0].id);
}

static void TestConversions()
{
    Check(Convert("MI", Value::Int(3), "Int To Float") == Value::Float(3.0),
          "int 3 -> float 3.0");
    Check(Convert("MF", Value::Float(3.9), "Float To Int") == Value::Int(3),
          "float 3.9 -> int 3 (truncate)");
    Check(Convert("MI", Value::Int(42), "Int To String") == Value::Str("42"),
          "int 42 -> string");
    Check(Convert("MB", Value::Bool(true), "Bool To String") == Value::Str("true"),
          "bool true -> string");
    Check(Convert("MI", Value::Int(0), "Int To Bool") == Value::Bool(false),
          "int 0 -> bool false");
    Check(Convert("MI", Value::Int(5), "Int To Bool") == Value::Bool(true),
          "int 5 -> bool true");
    Check(Convert("MS", Value::Str("17"), "String To Int") == Value::Int(17),
          "string 17 -> int");
}

static void TestSuggest()
{
    Check(SuggestConversion(TypeTag::Int, TypeTag::Float) == "Int To Float", "suggest int->float");
    Check(SuggestConversion(TypeTag::String, TypeTag::Int) == "String To Int", "suggest str->int");
    Check(SuggestConversion(TypeTag::Int, TypeTag::Int).empty(), "no suggestion for same type");
    Check(SuggestConversion(TypeTag::Bool, TypeTag::Float).empty(), "no bool->float converter");
}

int main()
{
    TestConversions();
    TestSuggest();
    if (failCount == 0) {
        std::printf("conversion_nodes_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
