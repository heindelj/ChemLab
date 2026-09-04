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
//   - `scenes`: one graph per scene (scene.h). A scene is a graph with a
//     Layout node (or several: its layouts), each arranging Panel nodes in
//     the slots of a layout; the active scene's active layout is what the
//     dockspace shows. `scene <name>` switches, `scene <name> graph` opens
//     the scene graph.

#include <map>
#include <string>
#include <vector>

#include "core/molecule.h"
#include "graph/data_store.h"
#include "graph/graph.h"
#include "graph/py_runner.h"
#include "graph/scene.h"
#include "graph/workflow.h"

struct AppState;
class CommandRegistry;

namespace graph {

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

// A window owned by one visualize node. A Render 3D or Plot 2D node in any
// graph gets its own floating, dockable window showing what arrived at the
// node: creating the node creates the window, deleting the node removes it,
// and the window can be closed and reopened from the node body. Evaluation
// only fills in the data here; the UI (ui/node_views.cpp) owns the render
// textures and GPU models, one set per window, keyed by the node's uid.
enum class NodeViewKind { View3D, Plot };

struct NodeView {
    NodeViewKind kind = NodeViewKind::View3D;
    uint64_t uid = 0;          // Node::uid of the owning node
    std::string title;         // window title (the node's title)
    bool open = true;          // the window is shown
    uint64_t version = 0;      // bumped whenever the data below changes
    // View3D
    ChemicalData atoms;
    std::string label;         // badge: "<structure> | frame i/n"
    // Plot
    plot::NamedPlot plot;
};

struct GraphSystem {
    Graph graph;
    CanvasGraph canvas;
    DataStore store;
    std::vector<Scene> scenes;                       // built-in first, then scenes/*.json
    int activeScene = 0;                             // index into `scenes`
    std::vector<Workflow> workflows;                 // built-in first, then workflows/*.json
    std::map<uint64_t, NodeView> nodeViews;          // node uid -> its window (see NodeView)
    std::string pythonExe = "python3";   // interpreter used by script nodes (the project's when one is open)
    ScriptEnv pythonEnv;                 // extra environment for them ([python] env)
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

    // ---- scenes ----
    // Built-in scenes plus scenes/*.json; called once at startup (UIInit).
    void LoadScenes(AppState& state);
    Scene& ActiveScene();
    int FindScene(const std::string& name) const;    // by scene name, -1 when absent
    // Make scene `index` the one the dockspace shows (re-docks on the next frame).
    bool SwitchScene(AppState& state, int index);
    // Show the scene, or the layout of any scene, called `name` (a scene
    // name wins over a layout name). False when neither exists.
    bool ShowLayout(AppState& state, const std::string& name);
    // Evaluate a scene graph into `store` (outputs under "scene/").
    std::string RunScene(AppState& state, Scene& scene);
    // Remove a user scene (and its file). Built-in scenes cannot be removed.
    bool RemoveScene(AppState& state, int index, std::string& err);

    // ---- workflows ----
    // Built-in workflows plus workflows/*.json; called with LoadScenes.
    void LoadWorkflows(AppState& state);
    int FindWorkflow(const std::string& name) const;   // -1 when absent
    // Compile (when needed) and execute a workflow; fills its run state and
    // returns the summary.
    std::string RunWorkflow(AppState& state, Workflow& w);
    // Remove a user workflow (and its file). Built-ins cannot be removed.
    bool RemoveWorkflow(int index, std::string& err);

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
// compares addresses): the Node Graph, the Graph Canvas or a scene graph.
const Graph* OwningGraph(const GraphSystem& gs, const Node& n);
Graph* OwningGraph(GraphSystem& gs, const Node& n);

// Where named canvas graphs live: "graphs/" in the working directory, or the
// open project's graphs folder (SetGraphsDir; "" restores the default).
// GraphPath("rdf") == "graphs/rdf.json"; SavedGraphNames lists what is there.
std::string GraphsDir();
void SetGraphsDir(const std::string& dir);
std::string GraphPath(const std::string& name);
std::vector<std::string> SavedGraphNames();

// `graph <run|new|save|load|list|demo|clear|...>`, `canvas <run|save|load|...>`
// and `scene <name|list|graph|new|save|...>` commands (graph_commands.cpp).
void RegisterGraphCommands(CommandRegistry& r);

// Called once per frame (main loop): re-runs the graph (and the canvas) at
// autoRunFps when their auto-run is on. A no-op otherwise (run-on-click only).
void UpdateGraphAutoRun(AppState& state);

}  // namespace graph
