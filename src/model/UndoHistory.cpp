// Snapshot-based undo/redo over a v2 Graph.
//
// Note: collapse/expand also mutate the class/function registries. Undo here
// restores only the graph, so undoing a collapse removes the Call node but
// leaves the generated function class/def registered (an unused orphan, not a
// correctness problem). Full project-level history would be needed to reverse
// the registry side effects.

#include "UndoHistory.h"

#include <utility>

namespace gau {

void UndoHistory::Record(const Graph& graph)
{
    past.push_back(graph);
    future.clear();
    if (past.size() > MAX_DEPTH) {
        past.erase(past.begin());
    }
}

bool UndoHistory::Undo(Graph& graph)
{
    if (past.empty()) {
        return false;
    }
    future.push_back(graph);
    graph = std::move(past.back());
    past.pop_back();
    return true;
}

bool UndoHistory::Redo(Graph& graph)
{
    if (future.empty()) {
        return false;
    }
    past.push_back(graph);
    graph = std::move(future.back());
    future.pop_back();
    return true;
}

void UndoHistory::Clear()
{
    past.clear();
    future.clear();
}

} // namespace gau
