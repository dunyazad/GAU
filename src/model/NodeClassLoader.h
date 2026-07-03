#pragma once

#include "GraphTypes.h"
#include "NodeClass.h"

#include <string>
#include <vector>

// Loads node class definitions from a JSON file and registers them in
// the NodeClass registry. Malformed entries are skipped and reported in
// outErrors; valid entries still load. Returns the number of classes
// successfully registered.
//
// Expected schema:
// {
//   "nodeClasses": [
//     {
//       "name": "Multiply",
//       "category": "Pure",  // Event|Function|FlowControl|Pure|Object|CustomObject
//       "pins": [
//         {"direction": "in", "type": "float", "name": "A"}
//         // direction: in|input|out|output
//         // type: exec|bool|int|float|string|object
//       ],
//       "properties": [
//         {"name": "Speed", "type": "float", "default": 1.5},
//         {"name": "Tags", "container": "array", "type": "string",
//          "default": ["a", "b"]},
//         {"name": "Ids", "container": "set", "type": "int", "default": [1, 2]},
//         {"name": "Scores", "container": "map", "keyType": "string",
//          "type": "int", "default": {"kim": 1}}
//         // container: none|array|set|map (default none)
//         // type/keyType: bool|int|float|string
//         // default optional: scalar / array / object per container
//       ]
//     }
//   ]
// }
int LoadNodeClassesFromFile(const std::string& path, std::vector<std::string>& outErrors);

// Appends one node class definition to the JSON file (created if
// missing), preserving existing entries. Returns false and sets
// outError on failure.
bool AppendNodeClassToFile(const std::string& path, const std::string& name,
                           const std::string& category, const std::vector<PinDef>& pins,
                           const std::vector<PropertyDef>& properties,
                           const std::string& execFnName, std::string& outError);

// Replaces the entry whose "name" equals oldName with the new
// definition. Returns false if the file is unreadable or has no such
// entry (e.g. the class was created in an older session's file).
bool UpdateNodeClassInFile(const std::string& path, const std::string& oldName,
                           const std::string& name, const std::string& category,
                           const std::vector<PinDef>& pins,
                           const std::vector<PropertyDef>& properties,
                           const std::string& execFnName, std::string& outError);
