#include "UndoStack.h"

bool UndoStack::Execute(std::unique_ptr<GraphCommand> command, NodeGraph& graph)
{
    if (command == nullptr || !command->Execute(graph)) {
        return false;
    }
    undoList.push_back(std::move(command));
    redoList.clear();
    return true;
}

bool UndoStack::Undo(NodeGraph& graph)
{
    if (undoList.empty()) {
        return false;
    }
    std::unique_ptr<GraphCommand> command = std::move(undoList.back());
    undoList.pop_back();
    command->Undo(graph);
    redoList.push_back(std::move(command));
    return true;
}

bool UndoStack::Redo(NodeGraph& graph)
{
    if (redoList.empty()) {
        return false;
    }
    std::unique_ptr<GraphCommand> command = std::move(redoList.back());
    redoList.pop_back();
    if (!command->Execute(graph)) {
        return false;
    }
    undoList.push_back(std::move(command));
    return true;
}
