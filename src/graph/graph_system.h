#pragma once
// graph::GraphSystem -- the single object the rest of ChemLab holds on to
// (behind a forward declaration + unique_ptr in AppState). Owns the node
// graphs and the store of node-generated data, so everything about their
// internals can change without touching the app.
//
// There are two kinds of graph:
//   - `graph`: the free-form graph shown in the Node Graph panel (demos,
//     scratch analyses, the Plot Lab).
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

struct GraphSystem {
    Graph graph;
    DataStore store;
    std::map<std::string, PanelGraph> panelGraphs;   // panel id -> its graph
    View3DRequest view3d;
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

    // The graph behind a panel, seeded with that panel's default graph on
    // first use (SeedPanelGraph).
    PanelGraph& Panel(AppState& state, const std::string& panelId);
    bool HasPanel(const std::string& panelId) const { return panelGraphs.count(panelId) != 0; }
    // Evaluate a panel graph if its inputs changed since the last run (or
    // `force`). Cheap to call every frame. Returns the panel's lastError.
    std::string RunPanel(AppState& state, const std::string& panelId, bool force = false);
    // Throw the panel graph away and rebuild the default one.
    void ResetPanel(AppState& state, const std::string& panelId);
};

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

// `graph <run|demo|clear|python|show|reset>` commands (graph_commands.cpp).
void RegisterGraphCommands(CommandRegistry& r);

// Called once per frame (main loop): re-runs the graph at autoRunFps when
// auto-run is on. A no-op when auto-run is off (run-on-click only).
void UpdateGraphAutoRun(AppState& state);

}  // namespace graph
