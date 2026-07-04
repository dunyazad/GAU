#include "core/TypeRegistry.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"
#include "model/UndoHistory.h"

#include <cstdio>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static NodeClass NodeCls()
{
    NodeClass c;
    c.name = "N";
    c.category = "Pure";
    return c;
}

static void TestUndoRedo()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    classes.Register(NodeCls());
    const NodeClass* cls = classes.Find("N");

    Graph g(t);
    UndoHistory history;

    history.Record(g);          // checkpoint: empty
    g.AddNode(*cls, 0, 0);      // {A}
    history.Record(g);          // checkpoint: {A}
    g.AddNode(*cls, 0, 0);      // {A, B}
    Check(g.Nodes().size() == 2, "two nodes after two adds");

    Check(history.Undo(g) && g.Nodes().size() == 1, "undo removes B");
    Check(history.Undo(g) && g.Nodes().empty(), "undo removes A");
    Check(!history.Undo(g), "nothing left to undo");

    Check(history.Redo(g) && g.Nodes().size() == 1, "redo restores A");
    Check(history.Redo(g) && g.Nodes().size() == 2, "redo restores B");
    Check(!history.Redo(g), "nothing left to redo");
}

static void TestRecordClearsRedo()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    classes.Register(NodeCls());
    const NodeClass* cls = classes.Find("N");

    Graph g(t);
    UndoHistory history;
    history.Record(g);
    g.AddNode(*cls, 0, 0);      // {A}
    history.Record(g);
    g.AddNode(*cls, 0, 0);      // {A, B}

    history.Undo(g);            // -> {A}, redo available
    Check(history.CanRedo(), "redo available after undo");

    history.Record(g);          // a new edit clears redo
    g.AddNode(*cls, 0, 0);      // {A, C}
    Check(!history.CanRedo(), "new edit clears redo history");
    Check(g.Nodes().size() == 2, "graph has A and C");
    Check(history.Undo(g) && g.Nodes().size() == 1, "undo the new edit");
}

int main()
{
    TestUndoRedo();
    TestRecordClearsRedo();
    if (failCount == 0) {
        std::printf("undo_history_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
