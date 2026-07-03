#pragma once

#include "GraphTypes.h"
#include "UndoStack.h"

class NodeClass;

// Creates a node of a given class at a canvas position.
class AddNodeCommand : public GraphCommand
{
public:
    AddNodeCommand(const NodeClass& nodeClass, float x, float y);

    bool Execute(NodeGraph& graph) override;
    void Undo(NodeGraph& graph) override;

private:
    const NodeClass* nodeClass;
    float x;
    float y;
    NodeId createdNodeId = INVALID_ID;
};
