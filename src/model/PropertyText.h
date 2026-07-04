#pragma once

#include "NodeClass.h"

#include <string>

// Text form of a property value for editing UIs: scalars as plain text,
// Array/Set as "a, b, c", Map as "key:value, key:value".
std::string PropertyValueToText(const PropertyDef& def, const PropertyValue& value);

// Parses the text form back into a typed value per the property's
// container/type. Returns false and sets outError on malformed input.
bool ParsePropertyValueText(const PropertyDef& def, const std::string& text,
                            PropertyValue& outValue, std::string& outError);

// Builds the default instance value for a property definition: scalar/
// container defaults from the def, or a struct value with each field set
// to its type default when the property type is a user struct.
PropertyValue MakeDefaultPropertyValue(const PropertyDef& def);
