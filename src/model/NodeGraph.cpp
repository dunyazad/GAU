#include "NodeGraph.h"
#include "Guid.h"
#include "PropertyText.h"

NodeId NodeGraph::AddNode(const NodeClass& nodeClass, float x, float y)
{
    Node node;
    node.id = nextNodeId++;
    node.guid = GenerateGuid();
    node.nodeClass = &nodeClass;
    node.x = x;
    node.y = y;
    for (const PinDef& pinDef : nodeClass.GetPins()) {
        AddPin(node, pinDef);
    }
    for (const PropertyDef& propertyDef : nodeClass.GetProperties()) {
        node.propertyValues.push_back(MakeDefaultPropertyValue(propertyDef));
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
    pin.typeName = pinDef.typeName;
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
        RemoveLinksTouchingNode(node);
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
            RemoveLinksTouchingNode(nodes[i]);
            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

void NodeGraph::RemoveLinksTouchingNode(const Node& node)
{
    auto pinBelongsToNode = [&node](PinId pinId) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) {
                return true;
            }
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) {
                return true;
            }
        }
        return false;
    };

    for (std::size_t i = links.size(); i > 0; --i) {
        const Link& link = links[i - 1];
        if (pinBelongsToNode(link.fromPinId) || pinBelongsToNode(link.toPinId)) {
            links.erase(links.begin() + static_cast<std::ptrdiff_t>(i - 1));
        }
    }
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

const Node* NodeGraph::FindNodeByGuid(const std::string& guid) const
{
    for (const Node& node : nodes) {
        if (node.guid == guid) {
            return &node;
        }
    }
    return nullptr;
}

const Pin* NodeGraph::FindPin(PinId pinId) const
{
    for (const Node& node : nodes) {
        for (const Pin& pin : node.inputs) {
            if (pin.id == pinId) {
                return &pin;
            }
        }
        for (const Pin& pin : node.outputs) {
            if (pin.id == pinId) {
                return &pin;
            }
        }
    }
    return nullptr;
}

const Node* NodeGraph::FindPinOwner(PinId pinId) const
{
    const Pin* pin = FindPin(pinId);
    if (pin == nullptr) {
        return nullptr;
    }
    return FindNode(pin->nodeId);
}

bool NodeGraph::NormalizeConnection(PinId pinA, PinId pinB,
                                    PinId& outOutputPin, PinId& outInputPin) const
{
    const Pin* a = FindPin(pinA);
    const Pin* b = FindPin(pinB);
    if (a == nullptr || b == nullptr || a->direction == b->direction) {
        return false;
    }
    outOutputPin = (a->direction == PinDirection::Output) ? pinA : pinB;
    outInputPin = (a->direction == PinDirection::Output) ? pinB : pinA;
    return true;
}

bool NodeGraph::CanConnect(PinId pinA, PinId pinB) const
{
    const Pin* a = FindPin(pinA);
    const Pin* b = FindPin(pinB);
    if (a == nullptr || b == nullptr) {
        return false;
    }
    if (a->nodeId == b->nodeId) {
        return false;
    }
    if (a->direction == b->direction) {
        return false;
    }
    if (a->type != b->type) {
        return false;
    }

    if (a->type == PinType::Exec) {
        const Pin* outputPin = (a->direction == PinDirection::Output) ? a : b;
        const Pin* inputPin = (a->direction == PinDirection::Output) ? b : a;
        // Adding output->input creates a cycle when the input's node
        // already reaches the output's node through exec links.
        if (ExecPathExists(inputPin->nodeId, outputPin->nodeId)) {
            return false;
        }
    }
    return true;
}

bool NodeGraph::ExecPathExists(NodeId fromNodeId, NodeId toNodeId) const
{
    if (fromNodeId == toNodeId) {
        return true;
    }
    std::vector<NodeId> pending;
    std::vector<NodeId> visited;
    pending.push_back(fromNodeId);

    while (!pending.empty()) {
        const NodeId current = pending.back();
        pending.pop_back();

        bool alreadyVisited = false;
        for (NodeId seen : visited) {
            if (seen == current) {
                alreadyVisited = true;
                break;
            }
        }
        if (alreadyVisited) {
            continue;
        }
        visited.push_back(current);

        for (const Link& link : links) {
            const Pin* fromPin = FindPin(link.fromPinId);
            if (fromPin == nullptr || fromPin->type != PinType::Exec
                || fromPin->nodeId != current) {
                continue;
            }
            const Pin* toPin = FindPin(link.toPinId);
            if (toPin == nullptr) {
                continue;
            }
            if (toPin->nodeId == toNodeId) {
                return true;
            }
            pending.push_back(toPin->nodeId);
        }
    }
    return false;
}

LinkId NodeGraph::AddLink(PinId outputPinId, PinId inputPinId)
{
    Link link;
    link.id = nextLinkId++;
    link.fromPinId = outputPinId;
    link.toPinId = inputPinId;
    links.push_back(link);
    return links.back().id;
}

bool NodeGraph::RemoveLink(LinkId linkId)
{
    for (std::size_t i = 0; i < links.size(); ++i) {
        if (links[i].id == linkId) {
            links.erase(links.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

Link* NodeGraph::FindLink(LinkId linkId)
{
    for (Link& link : links) {
        if (link.id == linkId) {
            return &link;
        }
    }
    return nullptr;
}

const Link* NodeGraph::FindLink(LinkId linkId) const
{
    for (const Link& link : links) {
        if (link.id == linkId) {
            return &link;
        }
    }
    return nullptr;
}

const Link* NodeGraph::FindLinkToInput(PinId inputPinId) const
{
    for (const Link& link : links) {
        if (link.toPinId == inputPinId) {
            return &link;
        }
    }
    return nullptr;
}

const Link* NodeGraph::FindLinkFromOutput(PinId outputPinId) const
{
    for (const Link& link : links) {
        if (link.fromPinId == outputPinId) {
            return &link;
        }
    }
    return nullptr;
}

bool NodeGraph::IsPinConnected(PinId pinId) const
{
    for (const Link& link : links) {
        if (link.fromPinId == pinId || link.toPinId == pinId) {
            return true;
        }
    }
    return false;
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
