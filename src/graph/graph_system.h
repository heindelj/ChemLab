#pragma once
// graph::GraphSystem -- the single object the rest of ChemLab holds on to
// (behind a forward declaration + unique_ptr in AppState). Owns the node
// graphs and the store of node-generated data, so everything about their
// internals can change without touching the app.
//
// There are three kinds of graph:
//   - `graph`: the free-form graph shown in the Node Graph panel (demos,
//     scratch analyses, the Plot Lab).
//   - `canvas`: the free-form graph shown in the Graph Canvas panel -- the
//     place to sketch build -> simulate -> analyze -> visualize pipelines.
//     Opened blank by `graph new`, saved by name under graphs/<name>.json
//     (graph_io.h) from the panel or `graph save/load`.
//   - one *panel graph* per UI panel, keyed by panel id ("structure_view",
//     "active_structure", ...). Every panel is, underneath, a small graph:
//     the 3D view is Structure -> Select Frame -> Render 3D, the Active
//     Structure list is Load Structure x N -> Structure List, and so on. The
//     panel evaluates its graph (RunPanel) before drawing and renders what
//     the graph's sink nodes produced. Panels that are not decomposed yet
//     are wrapped in a single node of their own type ("panel.<id>").
//     Right-click a panel's tab (or View > Panel graphs) to open the graph.

#include <map>
#include <string>
#include <vector>

#include "core/molecule.h"
#include "graph/data_store.h"
#include "graph/graph.h"

struct AppState;
class CommandRegistry;

namespace graph {

struct PanelGraph {
    Graph graph;
    uint64_t lastStamp = ~0ull;   // app-state + graph.version fingerprint of the last evaluation
    std::string lastError;        // "" = the last evaluation succeeded
    uint64_t runCount = 0;
};

// What the Render 3D node asked the Structure View to draw. Written by the
// node's evaluation, read by RebuildModel / the panel (see ViewAtoms).
struct View3DRequest {
    bool valid = false;
    Atoms atoms;
    std::string label;   // "<structure> | frame i/n", shown in the view badge
};

// The Graph Canvas panel's graph and its run state. Its outputs land in the
// shared store under a "canvas/" prefix so they never collide with `graph`'s.
struct CanvasGraph {
    Graph graph;
    std::string name = "untitled";        // saved as graphs/<name>.json (see GraphPath)
    std::string lastRunSummary;
    bool lastRunOk = true;
    uint64_t runCount = 0;
    bool autoRun = false;                 // re-run at GraphSystem::autoRunFps
    double lastAutoRun = 0.0;
    std::string lastIoMessage;            // result of the last save/load, shown in the panel
    bool lastIoOk = true;
};

// A window owned by one visualize node. A Render 3D or Plot 2D node placed
// in a free-form graph (rather than in the panel graph it normally drives)
// gets its own floating, dockable window showing what arrived at the node:
// creating the node creates the window, deleting the node removes it, and
// the window can be closed and reopened from the node body. Evaluation only
// fills in the data here; the UI (ui/node_views.cpp) owns the render
// textures and GPU models, one set per window, keyed by the node's uid.
enum class NodeViewKind { View3D, Plot };

struct NodeView {
    NodeViewKind kind = NodeViewKind::View3D;
    uint64_t uid = 0;          // Node::uid of the owning node
    std::string title;         // window title (the node's title)
    bool open = true;          // the window is shown
    uint64_t version = 0;      // bumped whenever the data below changes
    // View3D
    Atoms atoms;
    std::string label;         // badge: "<structure> | frame i/n"
    // Plot
    plot::NamedPlot plot;
};

struct GraphSystem {
    Graph graph;
    CanvasGraph canvas;
    DataStore store;
    std::map<std::string, PanelGraph> panelGraphs;   // panel id -> its graph
    View3DRequest view3d;
    std::map<uint64_t, NodeView> nodeViews;          // node uid -> its window (see NodeView)
    std::string pythonExe = "python3";   // interpreter used by script nodes
    bool autoRun = false;                // re-run `graph` continuously...
    float autoRunFps = 10.0f;            // ...at this rate (see UpdateGraphAutoRun)
    double lastAutoRun = 0.0;
    std::string lastRunSummary;          // multi-line, shown in the panel/console
    bool lastRunOk = true;
    uint64_t runCount = 0;

    // Evaluate `graph` into `store`; fills lastRunSummary/lastRunOk and
    // returns the summary.
    std::string Run(AppState& state);
    // Same for the Graph Canvas (fills canvas.lastRunSummary/lastRunOk).
    std::string RunCanvas(AppState& state);
    // Start a blank canvas called `name` (and open the panel).
    void NewCanvas(AppState& state, const std::string& name);
    // Save/load the canvas graph as graphs/<name>.json; the name is remembered
    // in canvas.name. Both fill canvas.lastIoMessage and return its success.
    bool SaveCanvas(const std::string& name);
    bool LoadCanvas(AppState& state, const std::string& name);

    // The graph behind a panel, seeded with that panel's default graph on
    // first use (SeedPanelGraph).
    PanelGraph& Panel(AppState& state, const std::string& panelId);
    bool HasPanel(const std::string& panelId) const { return panelGraphs.count(panelId) != 0; }
    // Evaluate a panel graph if its inputs changed since the last run (or
    // `force`). Cheap to call every frame. Returns the panel's lastError.
    std::string RunPanel(AppState& state, const std::string& panelId, bool force = false);
    // Throw the panel graph away and rebuild the default one.
    void ResetPanel(AppState& state, const std::string& panelId);

    // The node with this uid in any graph (null when it was deleted).
    Node* FindNodeByUid(uint64_t uid);
    // The window of a visualize node (null when it has none / was never run).
    NodeView* FindView(uint64_t uid);
    // The window for `n`, created (open) on first use with the given kind.
    NodeView& ViewFor(const Node& n, NodeViewKind kind);
    // Drop views whose node no longer exists. The UI calls this every frame
    // and releases the GPU side of anything that went away.
    void PruneViews();
};

// The graph a node lives in (nodes are stable in their deque, so this
// compares addresses): the Node Graph, the Graph Canvas or a panel graph.
const Graph* OwningGraph(const GraphSystem& gs, const Node& n);
Graph* OwningGraph(GraphSystem& gs, const Node& n);

// Build the default graph for a panel (nodes_view.cpp). Panels without a
// dedicated decomposition get one "panel.<id>" wrapper node when that node
// type is registered (the UI registers one per panel, see panel_registry.cpp).
void SeedPanelGraph(AppState& state, const std::string& panelId, Graph& g);

// Keep the Active Structure panel graph in step with structures loaded or
// removed outside the graph (File > Open, drag and drop, `load`, ...):
// every loaded file has a Load Structure node feeding the Structure List.
void OnStructureLoaded(AppState& state, int index);
void OnStructureRemoved(AppState& state, const std::string& path);

// The atoms the Structure View should draw, as requested by the Render 3D
// node of the structure_view graph; null when no node made a request (the
// view then falls back to the active frame).
const Atoms* ViewAtoms(AppState& state);

// Where named canvas graphs live: "graphs/" in the working directory.
// GraphPath("rdf") == "graphs/rdf.json"; SavedGraphNames lists what is there.
std::string GraphsDir();
std::string GraphPath(const std::string& name);
std::vector<std::string> SavedGraphNames();

// `graph <run|new|save|load|list|demo|clear|...>` and `canvas <run|save|load|...>`
// commands (graph_commands.cpp).
void RegisterGraphCommands(CommandRegistry& r);

// Called once per frame (main loop): re-runs the graph (and the canvas) at
// autoRunFps when their auto-run is on. A no-op otherwise (run-on-click only).
void UpdateGraphAutoRun(AppState& state);

}  // namespace graph
