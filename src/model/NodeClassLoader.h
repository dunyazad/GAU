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
//       "category": "Pure",            // Event|Function|FlowControl|Pure
//       "pins": [
//         {"direction": "in", "type": "float", "name": "A"}
//         // direction: in|input|out|output
//         // type: exec|bool|int|float|string|object
//       ]
//     }
//   ]
// }
int LoadNodeClassesFromFile(const std::string& path, std::vector<std::string>& outErrors);

// Appends one node class definition to the JSON file (created if
// missing), preserving existing entries. Returns false and sets
// outError on failure.
bool AppendNodeClassToFile(const std::string& path, const std::string& name,
                           NodeCategory category, const std::vector<PinDef>& pins,
                           std::string& outError);
