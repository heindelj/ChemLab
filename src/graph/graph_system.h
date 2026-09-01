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
    std::string lastRunSummary;          // multi-line, shown in the panel/console
    bool lastRunOk = true;
    uint64_t runCount = 0;

    // Evaluate `graph` into `store`; fills lastRunSummary/lastRunOk and
    // returns the summary.
    std::string Run(AppState& state);
};

// `graph <run|demo|clear|python>` commands (graph_commands.cpp).
void RegisterGraphCommands(CommandRegistry& r);

}  // namespace graph
