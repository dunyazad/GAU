#include "GraphCommands.h"
#include "NodeClass.h"
#include "NodeGraph.h"

AddNodeCommand::AddNodeCommand(const NodeClass& nodeClass, float x, float y)
    : nodeClass(&nodeClass)
    , x(x)
    , y(y)
{
}

bool AddNodeCommand::Execute(NodeGraph& graph)
{
    createdNodeId = graph.AddNode(*nodeClass, x, y);
    return createdNodeId != INVALID_ID;
}

void AddNodeCommand::Undo(NodeGraph& graph)
{
    graph.RemoveNode(createdNodeId);
    createdNodeId = INVALID_ID;
}
