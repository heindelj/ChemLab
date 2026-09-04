#include "graph/graph.h"

#include <algorithm>
#include <deque>

#include <fmt/format.h>

#include "graph/data_store.h"

namespace graph {

int FindInputPin(const Node& n, const std::string& name) {
    for (size_t i = 0; i < n.inputs.size(); ++i)
        if (n.inputs[i].name == name) return (int)i;
    return -1;
}

int FindOutputPin(const Node& n, const std::string& name) {
    for (size_t i = 0; i < n.outputs.size(); ++i)
        if (n.outputs[i].name == name) return (int)i;
    return -1;
}

uint64_t NextNodeUid() {
    static uint64_t next = 1;
    return next++;
}

Node* Graph::FindNode(uint32_t id) {
    for (auto& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

const Node* Graph::FindNode(uint32_t id) const {
    for (const auto& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

Node* Graph::AddNode(const std::string& typeId, float x, float y) {
    const NodeTypeSpec* spec = NodeTypes().Find(typeId);
    if (!spec) return nullptr;
    Node n;
    n.id = nextNodeId++;
    n.uid = NextNodeUid();
    n.typeId = typeId;
    n.title = fmt::format("{} {}", spec->name, n.id);
    n.inputs = spec->inputs;
    n.outputs = spec->outputs;
    n.outValues.resize(n.outputs.size());
    n.posX = x;
    n.posY = y;
    n.posDirty = true;
    nodes.push_back(std::move(n));
    Touch();
    return &nodes.back();
}

void Graph::RemoveNode(uint32_t nodeId) {
    links.erase(std::remove_if(links.begin(), links.end(),
                               [&](const Link& l) { return l.fromNode == nodeId || l.toNode == nodeId; }),
                links.end());
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const Node& n) { return n.id == nodeId; }),
                nodes.end());
    Touch();
}

bool Graph::WouldCycle(uint32_t fromNode, uint32_t toNode) const {
    // Adding fromNode -> toNode cycles iff fromNode is reachable from toNode.
    std::deque<uint32_t> queue{toNode};
    std::vector<uint32_t> seen;
    while (!queue.empty()) {
        const uint32_t cur = queue.front();
        queue.pop_front();
        if (cur == fromNode) return true;
        if (std::find(seen.begin(), seen.end(), cur) != seen.end()) continue;
        seen.push_back(cur);
        for (const Link& l : links)
            if (l.fromNode == cur) queue.push_back(l.toNode);
    }
    return false;
}

bool Graph::AddLink(uint32_t fromNode, int fromPin, uint32_t toNode, int toPin, std::string* err) {
    auto fail = [&](std::string msg) {
        if (err) *err = std::move(msg);
        return false;
    };
    const Node* from = FindNode(fromNode);
    const Node* to = FindNode(toNode);
    if (!from || !to) return fail("no such node");
    if (fromNode == toNode) return fail("cannot link a node to itself");
    if (fromPin < 0 || fromPin >= (int)from->outputs.size()) return fail("no such output pin");
    if (toPin < 0 || toPin >= (int)to->inputs.size()) return fail("no such input pin");
    const ValueType a = from->outputs[fromPin].type, b = to->inputs[toPin].type;
    if (!Compatible(a, b)) return fail(fmt::format("type mismatch: {} -> {}", TypeName(a), TypeName(b)));
    if (WouldCycle(fromNode, toNode)) return fail("would create a cycle");
    // One link per input: a new one replaces the old.
    links.erase(std::remove_if(links.begin(), links.end(),
                               [&](const Link& l) { return l.toNode == toNode && l.toPin == toPin; }),
                links.end());
    links.push_back({nextLinkId++, fromNode, fromPin, toNode, toPin});
    Touch();
    return true;
}

void Graph::RemoveLink(uint32_t linkId) {
    links.erase(std::remove_if(links.begin(), links.end(), [&](const Link& l) { return l.id == linkId; }),
                links.end());
    Touch();
}

void Graph::PruneLinks() {
    links.erase(std::remove_if(links.begin(), links.end(),
                               [&](const Link& l) {
                                   const Node* from = FindNode(l.fromNode);
                                   const Node* to = FindNode(l.toNode);
                                   return !from || !to || l.fromPin >= (int)from->outputs.size() ||
                                          l.toPin >= (int)to->inputs.size() ||
                                          !Compatible(from->outputs[l.fromPin].type, to->inputs[l.toPin].type);
                               }),
                links.end());
    Touch();
}

const Link* Graph::LinkInto(uint32_t nodeId, int pin) const {
    for (const Link& l : links)
        if (l.toNode == nodeId && l.toPin == pin) return &l;
    return nullptr;
}

void Graph::Clear() {
    nodes.clear();
    links.clear();
    nextNodeId = 1;
    nextLinkId = 1;
    Touch();
}

std::string Graph::Evaluate(AppState& state, DataStore& store, const std::string& keyPrefix) {
    if (const std::string err = EnsureCompiled(*this, program); !err.empty()) return err;
    lastStats = Execute(state, program);
    for (const Node& node : nodes) {
        if (!node.error.empty() || node.skipped) continue;
        for (size_t k = 0; k < node.outputs.size(); ++k)
            store.Set(fmt::format("{}{}.{}", keyPrefix, node.title, node.outputs[k].name), node.outValues[k]);
    }
    return lastStats.firstError;
}

}  // namespace graph
