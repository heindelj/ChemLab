#pragma once
// The catalog of node types, shaped like CommandRegistry / PanelCatalog:
// ChemLab's built-in nodes register here, and future third-party nodes (or
// script wrappers) are just more entries. UI-free except drawBody.

#include <functional>
#include <string>
#include <vector>

#include "graph/value.h"

struct AppState;

namespace graph {

struct Node;

struct PinSpec {
    std::string name;
    ValueType type = ValueType::Any;
};

// The coarse role of a node in a workflow. Drives the node colour in the
// canvas and the top-level grouping of the add-node menu; `category` below is
// the finer grouping inside a kind.
//   Build      -- creates input data, primarily molecular structures
//   Simulate   -- runs a simulation on input data, producing many kinds of output
//   Analyze    -- processes molecular (or other) data, often but not only from a simulation
//   Visualize  -- shows data; usually a ChemLab panel or plot
//   Other      -- glue that fits none of the above (scripts, notes, plumbing)
enum class NodeKind { Build, Simulate, Analyze, Visualize, Other };

const char* KindName(NodeKind k);                 // "build", "simulate", ...
bool KindFromName(const std::string& s, NodeKind& out);
// Display colour (r, g, b, a in 0..1): build orange, simulate purple, analyze
// green, visualize cyan, other grey.
struct KindColor { float r, g, b, a; };
KindColor ColorOf(NodeKind k);

// Evaluate one node. inputs[i] is the value arriving at input pin i (null when
// unconnected or empty); outputs is pre-sized to the node's output pins.
// Returns "" on success or an error message shown on the node.
// Must not call ImGui -- evaluation stays UI-free so it can go async later.
using EvalFn = std::function<std::string(AppState&, Node&, const std::vector<const Value*>&, std::vector<Value>&)>;

// Draw the node's parameter widgets inside the node body (already inside the
// editor canvas: keep to plain widgets, no popups/combos). Returns true when a
// parameter changed.
using BodyFn = std::function<bool(AppState&, Node&)>;

struct NodeTypeSpec {
    std::string id;                 // stable id ("core.active_frame")
    std::string name;               // display name ("Active Frame")
    NodeKind kind = NodeKind::Other; // build / simulate / analyze / visualize / other
    std::string category;           // add-node menu grouping inside the kind ("Sources", ...)
    std::string description;        // one-liner for the add-node menu
    std::vector<PinSpec> inputs;    // static pins copied onto new nodes;
    std::vector<PinSpec> outputs;   //   script nodes replace them per instance
    EvalFn evaluate;
    BodyFn drawBody;                // optional
};

class NodeTypeRegistry {
public:
    void Register(NodeTypeSpec spec);
    const NodeTypeSpec* Find(const std::string& id) const;
    const std::vector<NodeTypeSpec>& All() const { return types; }

private:
    std::vector<NodeTypeSpec> types;
};

// Global registry with the built-ins registered on first use.
NodeTypeRegistry& NodeTypes();

// Implemented in nodes_builtin.cpp / node_python.cpp:
void RegisterBuiltinNodes(NodeTypeRegistry& r);   // nodes_builtin.cpp: sources, small computes, Watch, Text
void RegisterPlotNodes(NodeTypeRegistry& r);   // nodes_plot.cpp: tables, series, named plots
void RegisterViewNodes(NodeTypeRegistry& r);   // nodes_view.cpp: the nodes behind the panels
void RegisterPythonNode(NodeTypeRegistry& r);
// Re-run `script --describe` for a python node and rebuild its pins
// (also used by `graph demo`). Returns "" or an error.
std::string DescribePythonNode(AppState& state, Node& node);

}  // namespace graph
