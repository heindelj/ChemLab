#pragma once
// graph::GraphSystem -- the single object the rest of ChemLab holds on to
// (behind a forward declaration + unique_ptr in AppState). Owns the node
// graph and the store of node-generated data, so everything about their
// internals can change without touching the app.

#include <string>

#include "graph/data_store.h"
#include "graph/graph.h"

struct AppState;
class CommandRegistry;

namespace graph {

struct GraphSystem {
    Graph graph;
    DataStore store;
    std::string pythonExe = "python3";   // interpreter used by script nodes
    bool autoRun = false;                // re-run the graph continuously...
    float autoRunFps = 10.0f;            // ...at this rate (see UpdateGraphAutoRun)
    double lastAutoRun = 0.0;
    std::string lastRunSummary;          // multi-line, shown in the panel/console
    bool lastRunOk = true;
    uint64_t runCount = 0;

    // Evaluate `graph` into `store`; fills lastRunSummary/lastRunOk and
    // returns the summary.
    std::string Run(AppState& state);
};

// `graph <run|demo|clear|python>` commands (graph_commands.cpp).
void RegisterGraphCommands(CommandRegistry& r);

// Called once per frame (main loop): re-runs the graph at autoRunFps when
// auto-run is on. A no-op when auto-run is off (run-on-click only).
void UpdateGraphAutoRun(AppState& state);

}  // namespace graph
