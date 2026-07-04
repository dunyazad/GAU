#pragma once

// Unified runtime value for v2: one Value type flows through exec links
// and stores property values, covering scalars, enums (as an index),
// structs, and containers. Pure data.

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace gau {

struct Value;

struct StructVal
{
    std::vector<Value> fields;
};

struct ArrayVal
{
    std::vector<Value> items;
};

struct MapVal
{
    std::vector<std::pair<Value, Value>> entries;
};

// std::monostate: Exec flow or an absent/opaque Object value.
using ValueData = std::variant<std::monostate, bool, std::int64_t, double, std::string,
                               StructVal, ArrayVal, MapVal>;

struct Value
{
    ValueData data;

    Value() : data(std::monostate{}) {}
    explicit Value(ValueData d) : data(std::move(d)) {}

    static Value None() { return Value(); }
    static Value Bool(bool v) { return Value(ValueData(v)); }
    static Value Int(std::int64_t v) { return Value(ValueData(v)); }
    static Value Float(double v) { return Value(ValueData(v)); }
    static Value Str(std::string v) { return Value(ValueData(std::move(v))); }

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }
};

// Human-readable form: scalars as text, structs as "{a, b}", arrays as
// "[a, b]", maps as "{k: v}". Enums show their index (names need the type).
std::string ValueToString(const Value& value);

} // namespace gau
