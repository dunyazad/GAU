#include "model/GraphSerializer.h"
#include "model/NodeClass.h"
#include "model/NodeGraph.h"

#include <cstdio>
#include <string>
#include <vector>

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static const NodeClass TestStatClass("Test Stat", "CustomObject", {
    {PinDirection::Input, PinType::Exec, ""},
}, {
    {"Speed", PropertyContainer::None, PinType::Float, PinType::String,
     Value(1.5), {}, {}},
    {"Tags", PropertyContainer::Array, PinType::String, PinType::String,
     Value(false), {Value(std::string("a"))}, {}},
    {"Scores", PropertyContainer::Map, PinType::Int, PinType::String,
     Value(false), {}, {{Value(std::string("kim")), Value(10)}}},
});

static void TestRoundTrip()
{
    NodeGraph source;
    const NodeId branchId = source.AddNode(*NodeClass::FindByName("Branch"), 100.0f, 50.0f);
    Check(branchId != INVALID_ID, "spawn Branch");

    const NodeId statId = source.AddNode(TestStatClass, -30.0f, 200.0f);
    Node* statNode = source.FindNode(statId);
    Check(statNode != nullptr, "spawn Test Stat");
    if (statNode != nullptr) {
        statNode->propertyValues[0].scalar = Value(3.25);
        statNode->propertyValues[1].elements.push_back(Value(std::string("b")));
        statNode->propertyValues[2].entries.push_back({Value(std::string("lee")), Value(20)});
    }

    source.AddComment("Group A", -10.0f, -10.0f, 500.0f, 400.0f);

    const std::string path = "serializer_test_graph.json";
    std::string saveError;
    Check(SaveGraphToFile(source, path, saveError), "save");

    NodeGraph loaded;
    std::vector<std::string> loadErrors;
    Check(LoadGraphFromFile(loaded, path, loadErrors), "load");
    Check(loadErrors.empty(), "load without entry errors");
    for (const std::string& error : loadErrors) {
        std::printf("  load error: %s\n", error.c_str());
    }

    Check(loaded.GetNodes().size() == 2, "node count");
    Check(loaded.GetComments().size() == 1, "comment count");

    const Node* loadedStat = nullptr;
    for (const Node& node : loaded.GetNodes()) {
        if (std::string(node.nodeClass->GetName()) == "Test Stat") {
            loadedStat = &node;
        }
    }
    Check(loadedStat != nullptr, "Test Stat restored");
    if (loadedStat != nullptr) {
        Check(loadedStat->x == -30.0f && loadedStat->y == 200.0f, "node position");
        Check(loadedStat->propertyValues.size() == 3, "property count");
        Check(loadedStat->propertyValues[0].scalar == Value(3.25), "scalar value");
        Check(loadedStat->propertyValues[1].elements.size() == 2, "array size");
        Check(loadedStat->propertyValues[2].entries.size() == 2, "map size");
    }

    const CommentNode& comment = loaded.GetComments().front();
    Check(comment.title == "Group A", "comment title");
    Check(comment.width == 500.0f && comment.height == 400.0f, "comment size");
}

int main()
{
    TestRoundTrip();

    if (failCount == 0) {
        std::printf("SerializerTests: all tests passed\n");
        return 0;
    }
    std::printf("SerializerTests: %d check(s) failed\n", failCount);
    return 1;
}
