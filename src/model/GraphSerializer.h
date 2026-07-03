#pragma once

#include <string>
#include <vector>

class NodeGraph;

// Saves the graph (nodes with class name, position and property values,
// plus comment boxes) as JSON. Links follow in M4/M5.
bool SaveGraphToFile(const NodeGraph& graph, const std::string& path, std::string& outError);

// Loads a graph saved by SaveGraphToFile into the (empty) graph. Nodes
// referencing unknown classes and malformed values are reported in
// outErrors and skipped; the rest of the file still loads. Returns
// false only when the file itself cannot be read/parsed.
bool LoadGraphFromFile(NodeGraph& graph, const std::string& path,
                       std::vector<std::string>& outErrors);
