#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class NodeGraph;

// Base class for undoable graph mutations. All user edits to the model
// must be expressed as commands and executed through UndoStack.
class GraphCommand
{
public:
    virtual ~GraphCommand() = default;

    // Applies the change. Returns false if it could not be applied;
    // failed commands are not recorded.
    virtual bool Execute(NodeGraph& graph) = 0;

    // Reverts the change made by the last successful Execute.
    virtual void Undo(NodeGraph& graph) = 0;
};

class UndoStack
{
public:
    // Executes the command and records it. Clears the redo history.
    bool Execute(std::unique_ptr<GraphCommand> command, NodeGraph& graph);

    // Returns false when there is nothing to undo/redo.
    bool Undo(NodeGraph& graph);
    bool Redo(NodeGraph& graph);

    // Number of commands currently applied; comparing against the depth
    // recorded at save time detects unsaved changes.
    std::size_t GetDepth() const { return undoList.size(); }

    // Monotonic counter bumped on every applied Execute/Undo/Redo. Unlike
    // GetDepth it never repeats, so callers can gate cached derivations
    // (e.g. the editor's data preview) on real state changes even across
    // undo-then-different-edit sequences that land on the same depth.
    std::uint64_t GetRevision() const { return revision; }

private:
    std::vector<std::unique_ptr<GraphCommand>> undoList;
    std::vector<std::unique_ptr<GraphCommand>> redoList;
    std::uint64_t revision = 0;
};
