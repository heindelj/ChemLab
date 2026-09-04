#pragma once
// Scenes. A *scene* is a graph that contains at least one Layout node
// ("scene.layout"). A Layout node picks a layout (how the dockspace is carved
// into slots, ui_spec.h) and has one input pin per slot; Panel nodes
// ("panel.<id>", one type per panel) plug into those pins, directly or
// through a Tabs node ("scene.tabs", several panels in one slot become
// tabs). The arrangement on screen is therefore just what is wired into the
// active Layout node -- the UI Builder edits the same wiring by drag and
// drop, and the node editor by hand; they are one thing done two ways.
//
// Every scene is a graph, but a graph is only a scene while it has a Layout
// node. The scene is named after its first Layout node; the graph itself can
// be called something else (Scene::graphName is the file stem). A scene may
// hold several Layout nodes: they are the scene's layouts, switched with
// `scene layout <name>` (or the Show button on the node), and they can share
// Panel nodes. A Layout node's layout cannot be changed while anything is
// wired into its slots.
//
// Built-in scenes: "classic" and "plot-lab".
// User scenes are saved as scenes/<graphName>.json (graph_io.h) and every
// graph in that folder with a Layout node is loaded at startup.

#include <string>
#include <vector>

#include "graph/graph.h"
#include "ui/ui_spec.h"

struct AppState;

namespace graph {

constexpr const char* kLayoutNodeType = "scene.layout";
constexpr const char* kTabsNodeType = "scene.tabs";
constexpr const char* kPanelNodePrefix = "panel.";
constexpr int kTabsPins = 6;

struct Scene {
    std::string graphName;        // file stem under scenes/ (the graph's name, not the scene's)
    Graph graph;
    uint32_t activeLayout = 0;    // node id of the Layout node shown (0 = the first one)
    bool builtin = false;
    bool graphOpen = false;       // the "Scene graph: <name>" window is shown
    uint64_t appliedLayout = ~0ull;    // LayoutStamp() last applied to the dockspace
    std::string lastRunSummary;
    bool lastRunOk = true;
    uint64_t runCount = 0;
    std::string lastIoMessage;
};

// The Layout nodes of a graph, in node order (empty: not a scene).
std::vector<Node*> LayoutNodes(Graph& g);
Node* LayoutNode(Graph& g);                 // the first one, null when none
const Node* LayoutNode(const Graph& g);
inline bool IsScene(const Graph& g) { return LayoutNode(g) != nullptr; }
// The Layout node the scene currently shows (activeLayout, else the first).
Node* ActiveLayoutNode(Scene& s);
const Node* ActiveLayoutNode(const Scene& s);
// A Layout node's name ("name" param; its title when unset).
std::string LayoutName(const Node& layout);
Node* FindLayoutNode(Graph& g, const std::string& name);
// Give a Layout node the slot pins its layout calls for (after the layout
// param changed, or a fresh/loaded node). Links to vanished pins are pruned.
void SyncLayoutPins(Graph& g, Node& layout);
// Is anything wired into the Layout node's slots? (Then its layout is locked.)
bool LayoutHasPanels(const Graph& g, const Node& layout);

// The scene's name: its first Layout node's name (graphName when none).
std::string SceneName(const Scene& s);
// Fingerprint of what the active Layout node shows: when it changes, the
// dockspace is rebuilt (ui.cpp). Unrelated edits in the graph leave it alone.
uint64_t LayoutStamp(const Scene& s);
// The arrangement wired into a Layout node (the active one by default),
// read statically from the links -- the graph need not have run.
UIDefinition LayoutUI(const Graph& g, const Node& layout);
UIDefinition SceneUI(const Scene& s);
// Rewire a Layout node (the active one, or a new one when `layout` is null)
// so it shows `ui`: Panel nodes are reused when the graph already has them
// (so other layouts keep sharing them), Tabs nodes are made for multi-panel
// slots, and Panel/Tabs nodes left feeding nothing are removed.
Node& SetLayoutUI(Graph& g, Node* layout, const UIDefinition& ui);
void SetSceneUI(Scene& s, const UIDefinition& ui);   // the active layout
// A scene whose graph is one Layout node showing `ui`, plus its panels.
Scene MakeScene(const UIDefinition& ui, bool builtin);
std::vector<Scene> BuiltinScenes();

// Where user scenes live: "scenes/" in the working directory, or the open
// project's scenes folder (SetScenesDir; "" restores the default).
std::string ScenesDir();
void SetScenesDir(const std::string& dir);
std::string ScenePath(const std::string& graphName);
bool SaveScene(Scene& s, std::string& err);                            // scenes/<graphName>.json
bool LoadSceneFile(const std::string& path, Scene& out, std::string& err);   // false when not a scene
std::vector<Scene> LoadUserScenes(std::vector<std::string>& errors);

// Where the active scene/layout is remembered between runs: the open
// project's [scene] table (marking it dirty), else chemlab_scene.toml in the
// working directory.
std::string ActiveSceneFile();
void ReadActiveScene(AppState& state, std::string& scene, std::string& layout);
void WriteActiveScene(AppState& state, const std::string& scene, const std::string& layout);

// The Layout, Tabs and Panel node types (registered with the other built-ins).
void RegisterSceneNodes(NodeTypeRegistry& r);

}  // namespace graph
