#pragma once
// Workflows: named graphs that ChemLab runs on its own, through the executor
// (executor.h), when something happens -- today, when the active frame
// changes -- or on demand. A workflow is a program expressed as a node graph:
// sources at one end (the active frame), native kernels in the middle,
// sinks at the other (write a topology back onto the frame).
//
// Triggers say *when* a workflow runs. Whether it needs to run is the
// graph's own business: the built-in covalent-bonds workflow starts with
// Has Topology -> Gate, so when the frame already carries a "bonds" topology
// the Gate closes and every node behind it is skipped. Caching therefore
// lives in the graph, not in the executor.
//
// Built-in workflows are constructed in code (BuiltinWorkflows); user
// workflows are workflows/<name>.json -- a graph document (graph_io.h) with an
// extra "workflow" object holding the trigger -- loaded at startup and listed
// in the Workflows panel. Double-clicking one there opens its graph.

#include <string>
#include <vector>

#include "graph/executor.h"
#include "graph/graph.h"

struct AppState;
class CommandRegistry;

namespace graph {

enum class Trigger {
    Manual,       // only when run from the panel / `workflow run`
    FrameChange,  // whenever the active frame (or structure) changes, and once at load
};
const char* TriggerName(Trigger t);                  // "manual", "frame"
bool TriggerFromName(const std::string& s, Trigger& out);
const char* TriggerDescription(Trigger t);

struct Workflow {
    std::string name;
    std::string description;
    Graph graph;
    Trigger trigger = Trigger::Manual;
    bool builtin = false;
    bool enabled = true;          // a disabled workflow ignores its trigger
    bool graphOpen = false;       // the "Workflow: <name>" graph window is shown
    // run state
    ExecStats lastStats;
    std::string lastRunSummary;   // one line per node that failed, else a short outcome
    bool lastRunOk = true;
    bool lastRunSkipped = false;  // the Gate closed: nothing to do
    uint64_t runCount = 0;
    std::string lastIoMessage;
};

// Where user workflows live: "workflows/" in the working directory, or the
// open project's folder (SetWorkflowsDir; "" restores the default).
std::string WorkflowsDir();
void SetWorkflowsDir(const std::string& dir);
std::string WorkflowPath(const std::string& name);

bool SaveWorkflow(const Workflow& w, std::string& err);                     // workflows/<name>.json
bool LoadWorkflowFile(const std::string& path, Workflow& out, std::string& err);
std::vector<Workflow> LoadUserWorkflows(std::vector<std::string>& errors);
std::vector<Workflow> BuiltinWorkflows();

// The built-in covalent bond detection, as a graph of native nodes:
//   Chemical Data -> Has Topology("bonds") -> Gate(missing)
//     -> Covalent Radii -> 2*max + tolerance -> Neighbor List
//     -> Gather(r_i), Gather(r_j) -> r_i + r_j + tolerance
//     -> Compare(d < cutoff) -> Filter(i), Filter(j) -> Apply Topology("bonds")
Workflow MakeCovalentBondsWorkflow();

// `workflow <list|run|graph|trigger|enable|new|save|delete|bench> ...`
void RegisterWorkflowCommands(CommandRegistry& r);

// Called once per frame (main loop): runs FrameChange workflows when the
// active frame changed since the last call.
void UpdateWorkflowTriggers(AppState& state);

}  // namespace graph
