// Value equality and string formatting.

#include "Value.h"

#include <cstdio>

namespace gau {

static bool DataEquals(const ValueData& a, const ValueData& b);

bool Value::operator==(const Value& other) const
{
    return DataEquals(data, other.data);
}

static bool StructEquals(const StructVal& a, const StructVal& b)
{
    if (a.fields.size() != b.fields.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.fields.size(); ++i) {
        if (!(a.fields[i] == b.fields[i])) {
            return false;
        }
    }
    return true;
}

static bool ArrayEquals(const ArrayVal& a, const ArrayVal& b)
{
    if (a.items.size() != b.items.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.items.size(); ++i) {
        if (!(a.items[i] == b.items[i])) {
            return false;
        }
    }
    return true;
}

static bool MapEquals(const MapVal& a, const MapVal& b)
{
    if (a.entries.size() != b.entries.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.entries.size(); ++i) {
        if (!(a.entries[i].first == b.entries[i].first)
            || !(a.entries[i].second == b.entries[i].second)) {
            return false;
        }
    }
    return true;
}

static bool DataEquals(const ValueData& a, const ValueData& b)
{
    if (a.index() != b.index()) {
        return false;
    }
    if (std::holds_alternative<std::monostate>(a)) {
        return true;
    }
    if (const bool* av = std::get_if<bool>(&a)) {
        return *av == std::get<bool>(b);
    }
    if (const std::int64_t* av = std::get_if<std::int64_t>(&a)) {
        return *av == std::get<std::int64_t>(b);
    }
    if (const double* av = std::get_if<double>(&a)) {
        return *av == std::get<double>(b);
    }
    if (const std::string* av = std::get_if<std::string>(&a)) {
        return *av == std::get<std::string>(b);
    }
    if (const StructVal* av = std::get_if<StructVal>(&a)) {
        return StructEquals(*av, std::get<StructVal>(b));
    }
    if (const ArrayVal* av = std::get_if<ArrayVal>(&a)) {
        return ArrayEquals(*av, std::get<ArrayVal>(b));
    }
    if (const MapVal* av = std::get_if<MapVal>(&a)) {
        return MapEquals(*av, std::get<MapVal>(b));
    }
    return false;
}

static std::string DoubleToString(double value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", value);
    return std::string(buffer);
}

std::string ValueToString(const Value& value)
{
    if (std::holds_alternative<std::monostate>(value.data)) {
        return "";
    }
    if (const bool* v = std::get_if<bool>(&value.data)) {
        return *v ? "true" : "false";
    }
    if (const std::int64_t* v = std::get_if<std::int64_t>(&value.data)) {
        return std::to_string(*v);
    }
    if (const double* v = std::get_if<double>(&value.data)) {
        return DoubleToString(*v);
    }
    if (const std::string* v = std::get_if<std::string>(&value.data)) {
        return *v;
    }
    if (const StructVal* v = std::get_if<StructVal>(&value.data)) {
        std::string out = "{";
        for (std::size_t i = 0; i < v->fields.size(); ++i) {
            if (i != 0) {
                out += ", ";
            }
            out += ValueToString(v->fields[i]);
        }
        out += "}";
        return out;
    }
    if (const ArrayVal* v = std::get_if<ArrayVal>(&value.data)) {
        std::string out = "[";
        for (std::size_t i = 0; i < v->items.size(); ++i) {
            if (i != 0) {
                out += ", ";
            }
            out += ValueToString(v->items[i]);
        }
        out += "]";
        return out;
    }
    if (const MapVal* v = std::get_if<MapVal>(&value.data)) {
        std::string out = "{";
        for (std::size_t i = 0; i < v->entries.size(); ++i) {
            if (i != 0) {
                out += ", ";
            }
            out += ValueToString(v->entries[i].first) + ": "
                 + ValueToString(v->entries[i].second);
        }
        out += "}";
        return out;
    }
    return "";
}

} // namespace gau
