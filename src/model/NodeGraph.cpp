#include "NodeGraph.h"

NodeId NodeGraph::AddNode(const NodeClass& nodeClass, float x, float y)
{
    Node node;
    node.id = nextNodeId++;
    node.nodeClass = &nodeClass;
    node.x = x;
    node.y = y;
    for (const PinDef& pinDef : nodeClass.GetPins()) {
        AddPin(node, pinDef);
    }
    for (const PropertyDef& propertyDef : nodeClass.GetProperties()) {
        PropertyValue value;
        value.scalar = propertyDef.defaultValue;
        value.elements = propertyDef.defaultElements;
        value.entries = propertyDef.defaultEntries;
        node.propertyValues.push_back(std::move(value));
    }
    nodes.push_back(node);
    return nodes.back().id;
}

void NodeGraph::AddPin(Node& node, const PinDef& pinDef)
{
    Pin pin;
    pin.id = nextPinId++;
    pin.nodeId = node.id;
    pin.direction = pinDef.direction;
    pin.type = pinDef.type;
    pin.name = pinDef.name;

    if (pinDef.direction == PinDirection::Input) {
        node.inputs.push_back(pin);
    } else {
        node.outputs.push_back(pin);
    }
}

void NodeGraph::RebuildNodesOfClass(const NodeClass& nodeClass)
{
    for (Node& node : nodes) {
        if (node.nodeClass != &nodeClass) {
            continue;
        }
        node.inputs.clear();
        node.outputs.clear();
        for (const PinDef& pinDef : nodeClass.GetPins()) {
            AddPin(node, pinDef);
        }
        node.propertyValues.clear();
        for (const PropertyDef& propertyDef : nodeClass.GetProperties()) {
            PropertyValue value;
            value.scalar = propertyDef.defaultValue;
            value.elements = propertyDef.defaultElements;
            value.entries = propertyDef.defaultEntries;
            node.propertyValues.push_back(std::move(value));
        }
    }
}

bool NodeGraph::RemoveNode(NodeId nodeId)
{
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].id == nodeId) {
            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

Node* NodeGraph::FindNode(NodeId nodeId)
{
    for (Node& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

const Node* NodeGraph::FindNode(NodeId nodeId) const
{
    for (const Node& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

CommentId NodeGraph::AddComment(const std::string& title, float x, float y,
                                float width, float height)
{
    CommentNode comment;
    comment.id = nextCommentId++;
    comment.title = title;
    comment.x = x;
    comment.y = y;
    comment.width = width;
    comment.height = height;
    comments.push_back(comment);
    return comments.back().id;
}

bool NodeGraph::RemoveComment(CommentId commentId)
{
    for (std::size_t i = 0; i < comments.size(); ++i) {
        if (comments[i].id == commentId) {
            comments.erase(comments.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

CommentNode* NodeGraph::FindComment(CommentId commentId)
{
    for (CommentNode& comment : comments) {
        if (comment.id == commentId) {
            return &comment;
        }
    }
    return nullptr;
}

const CommentNode* NodeGraph::FindComment(CommentId commentId) const
{
    for (const CommentNode& comment : comments) {
        if (comment.id == commentId) {
            return &comment;
        }
    }
    return nullptr;
}
