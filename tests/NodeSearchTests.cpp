#include "core/TypeRegistry.h"
#include "interaction/NodeSearch.h"
#include "model/Graph.h"
#include "model/NodeClassV2.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace gau;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static NodeClass Simple(std::string name)
{
    NodeClass c;
    c.name = std::move(name);
    c.category = "Pure";
    return c;
}

static void TestSearch()
{
    TypeRegistry t;
    NodeClassRegistry classes;
    classes.Register(Simple("MakeInt"));
    classes.Register(Simple("MakeFloat"));
    classes.Register(Simple("PrintInt"));

    Graph g(t);
    g.AddNode(*classes.Find("MakeInt"), 0, 0);
    g.AddNode(*classes.Find("MakeFloat"), 0, 0);
    g.AddNode(*classes.Find("PrintInt"), 0, 0);

    Check(SearchNodes(g, "make").size() == 2, "'make' matches two nodes (case-insensitive)");
    Check(SearchNodes(g, "Int").size() == 2, "'Int' matches MakeInt and PrintInt");
    Check(SearchNodes(g, "float").size() == 1, "'float' matches one node");
    Check(SearchNodes(g, "zzz").empty(), "no match yields empty");
    Check(SearchNodes(g, "").empty(), "empty query matches nothing");
}

static void TestBounds()
{
    std::vector<NodeBox> boxes = {
        NodeBox{1, 10.0f, 20.0f, 100.0f, 40.0f},
        NodeBox{2, -30.0f, 200.0f, 50.0f, 30.0f},
    };
    Bounds b;
    Check(ComputeBounds(boxes, b), "bounds computed for non-empty");
    Check(std::fabs(b.minX + 30.0f) < 0.001f && std::fabs(b.minY - 20.0f) < 0.001f,
          "bounds min correct");
    Check(std::fabs(b.maxX - 110.0f) < 0.001f && std::fabs(b.maxY - 230.0f) < 0.001f,
          "bounds max correct");

    Bounds empty;
    Check(!ComputeBounds({}, empty), "empty set has no bounds");
}

int main()
{
    TestSearch();
    TestBounds();
    if (failCount == 0) {
        std::printf("node_search_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
