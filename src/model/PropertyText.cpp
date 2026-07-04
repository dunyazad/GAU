#include "PropertyText.h"

#include "UserType.h"

#include <cctype>
#include <utility>
#include <variant>

static std::string TrimAscii(const std::string& text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

static std::vector<std::string> SplitTrimmed(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == separator) {
            parts.push_back(TrimAscii(current));
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(TrimAscii(current));
    return parts;
}

// Default instance value for a (type, typeName) pair. Structs recurse into
// their fields; other types use their zero value.
static PropertyValue MakeDefaultForType(PinType type, const std::string& typeName)
{
    PropertyValue value;
    if (type == PinType::UserType) {
        const UserType* userType = UserTypeRegistry::Find(typeName);
        if (userType != nullptr && userType->kind == UserTypeKind::Struct) {
            for (const StructField& field : userType->fields) {
                value.structFields.push_back(MakeDefaultForType(field.type, field.typeName));
            }
            return value;
        }
    }
    value.scalar = MakeDefaultValue(type);
    return value;
}

PropertyValue MakeDefaultPropertyValue(const PropertyDef& def)
{
    PropertyValue value;
    if (def.container == PropertyContainer::None) {
        if (def.type == PinType::UserType) {
            const UserType* userType = UserTypeRegistry::Find(def.typeName);
            if (userType != nullptr && userType->kind == UserTypeKind::Struct) {
                for (const StructField& field : userType->fields) {
                    value.structFields.push_back(MakeDefaultForType(field.type, field.typeName));
                }
                return value;
            }
        }
        value.scalar = def.defaultValue;
        return value;
    }
    value.elements = def.defaultElements;
    value.entries = def.defaultEntries;
    return value;
}

std::string PropertyValueToText(const PropertyDef& def, const PropertyValue& value)
{
    switch (def.container) {
    case PropertyContainer::None:
        if (def.type == PinType::UserType) {
            const UserType* userType = UserTypeRegistry::Find(def.typeName);
            if (userType != nullptr && userType->kind == UserTypeKind::Struct) {
                // Structs are edited field-by-field; summarize on one line.
                return "{...}";
            }
            // Enum scalars display as the enumerator name for their index.
            const int* index = std::get_if<int>(&value.scalar);
            if (userType != nullptr && userType->kind == UserTypeKind::Enum && index != nullptr
                && *index >= 0 && *index < static_cast<int>(userType->enumerators.size())) {
                return userType->enumerators[static_cast<std::size_t>(*index)];
            }
        }
        return ValueToString(value.scalar);

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        std::string joined;
        for (const Value& element : value.elements) {
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += ValueToString(element);
        }
        return joined;
    }

    case PropertyContainer::Map: {
        std::string joined;
        for (const std::pair<Value, Value>& entry : value.entries) {
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += ValueToString(entry.first) + ":" + ValueToString(entry.second);
        }
        return joined;
    }
    }
    return std::string();
}

bool ParsePropertyValueText(const PropertyDef& def, const std::string& text,
                            PropertyValue& outValue, std::string& outError)
{
    const std::string trimmed = TrimAscii(text);
    outValue = PropertyValue();

    switch (def.container) {
    case PropertyContainer::None:
        if (!ParseValueString(trimmed, def.type, outValue.scalar)) {
            outError = "invalid value: " + trimmed;
            return false;
        }
        return true;

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        if (trimmed.empty()) {
            return true;
        }
        for (const std::string& part : SplitTrimmed(trimmed, ',')) {
            Value element;
            if (!ParseValueString(part, def.type, element)) {
                outError = "invalid element: " + part;
                return false;
            }
            if (def.container == PropertyContainer::Set) {
                bool duplicate = false;
                for (const Value& existing : outValue.elements) {
                    if (existing == element) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    continue;
                }
            }
            outValue.elements.push_back(std::move(element));
        }
        return true;
    }

    case PropertyContainer::Map: {
        if (trimmed.empty()) {
            return true;
        }
        for (const std::string& part : SplitTrimmed(trimmed, ',')) {
            const std::size_t separator = part.find(':');
            if (separator == std::string::npos) {
                outError = "map entry must be key:value: " + part;
                return false;
            }
            Value key;
            if (!ParseValueString(TrimAscii(part.substr(0, separator)), def.keyType, key)) {
                outError = "invalid map key in: " + part;
                return false;
            }
            for (const std::pair<Value, Value>& existing : outValue.entries) {
                if (existing.first == key) {
                    outError = "duplicate map key in: " + part;
                    return false;
                }
            }
            Value value;
            if (!ParseValueString(TrimAscii(part.substr(separator + 1)), def.type, value)) {
                outError = "invalid map value in: " + part;
                return false;
            }
            outValue.entries.emplace_back(std::move(key), std::move(value));
        }
        return true;
    }
    }
    outError = "unsupported container";
    return false;
}
