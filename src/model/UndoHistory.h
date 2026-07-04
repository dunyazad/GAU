#pragma once

// v2 undo/redo via graph snapshots (CLAUDE.md sec.6: all model edits must be
// undoable). A command-per-op scheme is awkward here because higher-level
// edits (collapse/expand) touch several structures at once; snapshotting the
// whole Graph before each edit is simple and always correct. Registry-side
// effects of collapse/expand are not captured -- see note in the .cpp.
//
// File name avoids the v1 UndoStack.h (which targets the v1 NodeGraph); this
// type lives in namespace gau.

#include "Graph.h"

#include <cstddef>
#include <vector>

namespace gau {

class UndoHistory
{
public:
    // Records the current graph as a checkpoint BEFORE applying an edit.
    // Clears the redo history. Older checkpoints past the cap are dropped.
    void Record(const Graph& graph);

    bool CanUndo() const { return !past.empty(); }
    bool CanRedo() const { return !future.empty(); }

    // Restores the previous/next checkpoint into `graph`. Returns false when
    // there is nothing to undo/redo.
    bool Undo(Graph& graph);
    bool Redo(Graph& graph);

    void Clear();
    std::size_t UndoDepth() const { return past.size(); }
    std::size_t RedoDepth() const { return future.size(); }

private:
    static constexpr std::size_t MAX_DEPTH = 100;
    std::vector<Graph> past;
    std::vector<Graph> future;
};

} // namespace gau
