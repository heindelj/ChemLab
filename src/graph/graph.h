#pragma once
// The node graph data model and its (synchronous, for now) evaluation.
// UI-free: the Node Graph panel draws it, commands drive it, and evaluation
// only reads AppState data and writes the DataStore.

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "graph/node_registry.h"
#include "graph/value.h"

struct AppState;

namespace graph {

class DataStore;

struct Node {
    uint32_t id = 0;
    std::string typeId;
    std::string title;                    // display name; also the DataStore key prefix
    std::vector<PinSpec> inputs;          // per-node copy (script nodes rebuild them)
    std::vector<PinSpec> outputs;
    std::map<std::string, Value> params;  // widget state ("i", "script", ...)
    std::vector<Value> outValues;         // last evaluation, parallel to outputs
    std::string error;                    // last evaluation error ("" = ok)
    float posX = 0, posY = 0;
    bool posDirty = false;                // panel should push posX/posY into the editor
};

struct Link {
    uint32_t id = 0;
    uint32_t fromNode = 0;                // output side
    int fromPin = 0;
    uint32_t toNode = 0;                  // input side
    int toPin = 0;
};

// Editor-facing pin ids: unique across the graph and decodable. Room for 49
// pins per side per node.
inline uint64_t InPinId(uint32_t nodeId, int pin) { return (uint64_t)nodeId * 100 + 1 + pin; }
inline uint64_t OutPinId(uint32_t nodeId, int pin) { return (uint64_t)nodeId * 100 + 51 + pin; }
inline void DecodePin(uint64_t pinId, uint32_t& nodeId, int& pin, bool& isOutput) {
    nodeId = (uint32_t)(pinId / 100);
    const int rem = (int)(pinId % 100);
    isOutput = rem >= 51;
    pin = isOutput ? rem - 51 : rem - 1;
}

int FindInputPin(const Node& n, const std::string& name);   // -1 when absent
int FindOutputPin(const Node& n, const std::string& name);

struct Graph {
    // deque: references to nodes stay valid while nodes are appended, so
    // callers (the demo builder, the panel) may hold Node* across AddNode.
    std::deque<Node> nodes;
    std::vector<Link> links;
    uint32_t nextNodeId = 1;
    uint32_t nextLinkId = 1;

    Node* FindNode(uint32_t id);
    const Node* FindNode(uint32_t id) const;
    Node* AddNode(const std::string& typeId, float x, float y);   // null on unknown type
    void RemoveNode(uint32_t nodeId);
    // Validates pins, types and acyclicity; replaces any existing link into the
    // same input. False + *err on rejection.
    bool AddLink(uint32_t fromNode, int fromPin, uint32_t toNode, int toPin, std::string* err = nullptr);
    void RemoveLink(uint32_t linkId);
    // Drop links referring to pins that no longer exist (script pins changed).
    void PruneLinks();
    const Link* LinkInto(uint32_t nodeId, int pin) const;
    bool WouldCycle(uint32_t fromNode, uint32_t toNode) const;
    void Clear();

    // Evaluate every node in dependency order and publish outputs into `store`
    // as "<title>.<pin>". Returns "" or a description of the first error.
    std::string Evaluate(AppState& state, DataStore& store);
};

}  // namespace graph
