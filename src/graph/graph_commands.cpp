// GraphSystem::Run plus the `graph` command family, so the node graph is fully
// drivable from the command bar (and therefore from --run smoke tests).

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "raylib.h"   // GetTime, for the auto-run clock

#include "app/app_state.h"
#include "app/commands.h"
#include "core/molecule.h"
#include "graph/graph_io.h"
#include "graph/graph_system.h"
#include "graph/py_runner.h"

namespace graph {

namespace {

// Evaluate `g` into `store` and describe the outcome, one line per output
// (or per failing node). `ok` reports whether every node evaluated.
std::string RunGraph(AppState& state, Graph& g, DataStore& store, const std::string& keyPrefix, bool& ok) {
    const std::string err = g.Evaluate(state, store, keyPrefix);
    std::string summary;
    for (const Node& n : g.nodes) {
        if (!n.error.empty()) {
            summary += fmt::format("{}: error: {}\n", n.title, n.error);
            continue;
        }
        for (size_t k = 0; k < n.outputs.size(); ++k)
            summary += fmt::format("{}.{} = {}\n", n.title, n.outputs[k].name, n.outValues[k].Preview());
    }
    if (!summary.empty() && summary.back() == '\n') summary.pop_back();
    if (summary.empty()) summary = "graph is empty";
    ok = err.empty();
    return summary;
}

}  // namespace

std::string GraphSystem::Run(AppState& state) {
    ++runCount;
    lastRunSummary = RunGraph(state, graph, store, "", lastRunOk);
    return lastRunSummary;
}

std::string GraphSystem::RunCanvas(AppState& state) {
    ++canvas.runCount;
    canvas.lastRunSummary = RunGraph(state, canvas.graph, store, "canvas/", canvas.lastRunOk);
    return canvas.lastRunSummary;
}

std::string GraphsDir() { return "graphs"; }

std::string GraphPath(const std::string& name) { return GraphsDir() + "/" + name + ".json"; }

std::vector<std::string> SavedGraphNames() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(GraphsDir(), ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

namespace {

// A graph name is a file stem: keep it to something every filesystem accepts.
std::string CleanGraphName(std::string name) {
    for (char& c : name)
        if (c == '/' || c == '\\' || c == ':' || c == '"' || c == '<' || c == '>' || c == '|' || c == '?' || c == '*')
            c = '_';
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();
    return name.empty() ? "untitled" : name;
}

}  // namespace

void GraphSystem::NewCanvas(AppState& state, const std::string& name) {
    canvas.graph.Clear();
    canvas.name = CleanGraphName(name);
    canvas.lastRunSummary.clear();
    canvas.runCount = 0;
    canvas.lastIoMessage = fmt::format("new graph '{}' (unsaved)", canvas.name);
    canvas.lastIoOk = true;
    state.PanelOpen("graph_canvas") = true;
}

bool GraphSystem::SaveCanvas(const std::string& nameIn) {
    const std::string name = CleanGraphName(nameIn);
    std::error_code ec;
    std::filesystem::create_directories(GraphsDir(), ec);
    std::string err;
    canvas.lastIoOk = SaveGraph(canvas.graph, GraphPath(name), err);
    canvas.lastIoMessage = canvas.lastIoOk
                               ? fmt::format("saved '{}' ({} node{}) to {}", name, canvas.graph.nodes.size(),
                                             canvas.graph.nodes.size() == 1 ? "" : "s", GraphPath(name))
                               : err;
    if (canvas.lastIoOk) canvas.name = name;
    return canvas.lastIoOk;
}

bool GraphSystem::LoadCanvas(AppState& state, const std::string& nameIn) {
    const std::string name = CleanGraphName(nameIn);
    std::string err;
    canvas.lastIoOk = LoadGraph(GraphPath(name), canvas.graph, err);
    canvas.lastIoMessage = canvas.lastIoOk
                               ? fmt::format("loaded '{}' ({} node{})", name, canvas.graph.nodes.size(),
                                             canvas.graph.nodes.size() == 1 ? "" : "s")
                               : err;
    if (canvas.lastIoOk) {
        canvas.name = name;
        canvas.lastRunSummary.clear();
        canvas.runCount = 0;
        state.PanelOpen("graph_canvas") = true;
    }
    return canvas.lastIoOk;
}

void UpdateGraphAutoRun(AppState& state) {
    GraphSystem& gs = state.GraphSys();
    const double now = GetTime();
    const double period = 1.0 / std::clamp(gs.autoRunFps, 0.1f, 120.0f);
    if (gs.autoRun && !gs.graph.nodes.empty() && now - gs.lastAutoRun >= period) {
        gs.lastAutoRun = now;
        gs.Run(state);
    }
    if (gs.canvas.autoRun && !gs.canvas.graph.nodes.empty() && now - gs.canvas.lastAutoRun >= period) {
        gs.canvas.lastAutoRun = now;
        gs.RunCanvas(state);
    }
}

namespace {

CommandResult BuildDistanceDemo(AppState& s) {
    GraphSystem& gs = s.GraphSys();
    gs.graph.Clear();

    Node* src = gs.graph.AddNode("core.active_frame", 60, 80);
    Node* pair = gs.graph.AddNode("core.atom_pair", 60, 280);
    Node* py = gs.graph.AddNode("script.python", 380, 140);
    Node* watch = gs.graph.AddNode("core.watch", 700, 170);
    if (!src || !pair || !py || !watch) return CommandResult::Error("built-in node types missing");

    const std::string script = std::string(ASSETS_PATH) + "scripts/atom_distance.py";
    py->params["script"] = Value::S(script);
    if (std::string err = DescribePythonNode(s, *py); !err.empty())
        return CommandResult::Error(fmt::format("describe failed for {}: {}", script, err));

    auto linkByName = [&](Node& from, const char* out, Node& to, const char* in) -> std::string {
        const int o = FindOutputPin(from, out), i = FindInputPin(to, in);
        if (o < 0 || i < 0) return fmt::format("missing pin {} -> {}", out, in);
        std::string err;
        if (!gs.graph.AddLink(from.id, o, to.id, i, &err)) return err;
        return "";
    };
    for (const std::string& err :
         {linkByName(*src, "positions", *py, "positions"), linkByName(*pair, "i", *py, "i"),
          linkByName(*pair, "j", *py, "j"), linkByName(*py, "distance", *watch, "value")})
        if (!err.empty()) return CommandResult::Error("demo wiring failed: " + err);

    std::string msg = "Demo graph created: Active Frame + Atom Pair -> atom_distance.py -> Watch. Run with `graph run`.";
    if (const Atoms* a = s.ActiveAtoms(); a && a->natoms >= 2) {
        // C++ reference value for the same pair, to check the script against.
        const float d = Distance(a->xyz[0], a->xyz[1]);
        msg += fmt::format("\nReference C++ distance, atoms 1-2: {:.6f} A", d);
    }
    s.PanelOpen("node_graph") = true;
    return CommandResult::Ok(msg);
}

CommandResult BuildHighlightDemo(AppState& s) {
    GraphSystem& gs = s.GraphSys();
    gs.graph.Clear();

    Node* chem = gs.graph.AddNode("core.chemical_data", 60, 80);
    Node* idx = gs.graph.AddNode("core.atom_index", 60, 260);
    Node* bonded = gs.graph.AddNode("core.bonded_atoms", 330, 220);
    Node* dim = gs.graph.AddNode("core.float", 60, 400);
    Node* hi = gs.graph.AddNode("core.highlight_alpha", 600, 140);
    Node* apply = gs.graph.AddNode("core.apply_atom_alpha", 880, 200);
    if (!chem || !idx || !bonded || !dim || !hi || !apply) return CommandResult::Error("built-in node types missing");
    dim->params["value"] = Value::F(0.2);
    // Seed the picked atom from the selection when there is one (1-based param).
    if (!s.selected.empty()) idx->params["i"] = Value::I(*s.selected.begin() + 1);

    auto linkByName = [&](Node& from, const char* out, Node& to, const char* in) -> std::string {
        const int o = FindOutputPin(from, out), i = FindInputPin(to, in);
        if (o < 0 || i < 0) return fmt::format("missing pin {} -> {}", out, in);
        std::string err;
        if (!gs.graph.AddLink(from.id, o, to.id, i, &err)) return err;
        return "";
    };
    for (const std::string& err :
         {linkByName(*chem, "chem", *bonded, "chem"), linkByName(*idx, "i", *bonded, "i"),
          linkByName(*chem, "chem", *hi, "chem"), linkByName(*bonded, "indices", *hi, "indices"),
          linkByName(*dim, "value", *hi, "alpha"), linkByName(*hi, "alphas", *apply, "alphas")})
        if (!err.empty()) return CommandResult::Error("demo wiring failed: " + err);

    s.PanelOpen("node_graph") = true;
    return CommandResult::Ok(
        "Highlight demo created: Atom Index -> Bonded Atoms (bonds topology) -> Highlight Alpha -> Apply Atom "
        "Alpha; atoms not bonded to the picked atom are dimmed. `graph run` (or Auto) applies it.");
}

// Load Table -> Column x3 -> Series -> three named plots, shown in the
// "Plot Lab" UI (2D plot on top, this graph below) so the picker in the plot
// pane switches between them.
CommandResult BuildPlotsDemo(AppState& s) {
    GraphSystem& gs = s.GraphSys();
    gs.graph.Clear();
    s.ClearPlots();

    Node* load = gs.graph.AddNode("data.load_table", 40, 60);
    Node* time = gs.graph.AddNode("data.column", 340, 40);
    Node* temp = gs.graph.AddNode("data.column", 340, 250);
    Node* pot = gs.graph.AddNode("data.column", 340, 460);
    Node* kin = gs.graph.AddNode("data.column", 340, 670);
    Node* sTemp = gs.graph.AddNode("plot.series", 640, 40);
    Node* sPot = gs.graph.AddNode("plot.series", 640, 260);
    Node* sKin = gs.graph.AddNode("plot.series", 640, 480);
    Node* sHist = gs.graph.AddNode("plot.series", 640, 700);
    Node* pTemp = gs.graph.AddNode("plot.plot2d", 960, 40);
    Node* pEnergy = gs.graph.AddNode("plot.plot2d", 960, 330);
    Node* pHist = gs.graph.AddNode("plot.plot2d", 960, 620);
    if (!load || !time || !temp || !pot || !kin || !sTemp || !sPot || !sKin || !sHist || !pTemp || !pEnergy || !pHist)
        return CommandResult::Error("data/plot node types missing");

    load->params["path"] = Value::S(std::string(ASSETS_PATH) + "data/md_demo.csv");
    time->params["column"] = Value::S("time_ps");
    temp->params["column"] = Value::S("temperature_K");
    pot->params["column"] = Value::S("potential_E");
    kin->params["column"] = Value::S("kinetic_E");
    sTemp->params["kind"] = Value::S("scatter");
    sPot->params["kind"] = Value::S("line");
    sKin->params["kind"] = Value::S("line");
    sHist->params["kind"] = Value::S("histogram");
    sHist->params["bins"] = Value::I(25);
    sHist->params["label"] = Value::S("T samples");
    pTemp->params["name"] = Value::S("Temperature");
    pTemp->params["xlabel"] = Value::S("time (ps)");
    pTemp->params["ylabel"] = Value::S("T (K)");
    pEnergy->params["name"] = Value::S("Energies");
    pEnergy->params["xlabel"] = Value::S("time (ps)");
    pEnergy->params["ylabel"] = Value::S("E (kcal/mol)");
    pHist->params["name"] = Value::S("Temperature histogram");
    pHist->params["xlabel"] = Value::S("T (K)");
    pHist->params["ylabel"] = Value::S("count");

    auto linkByName = [&](Node& from, const char* out, Node& to, const char* in) -> std::string {
        const int o = FindOutputPin(from, out), i = FindInputPin(to, in);
        if (o < 0 || i < 0) return fmt::format("missing pin {} -> {}", out, in);
        std::string err;
        if (!gs.graph.AddLink(from.id, o, to.id, i, &err)) return err;
        return "";
    };
    for (const std::string& err :
         {linkByName(*load, "table", *time, "table"), linkByName(*load, "table", *temp, "table"),
          linkByName(*load, "table", *pot, "table"), linkByName(*load, "table", *kin, "table"),
          linkByName(*time, "values", *sTemp, "x"), linkByName(*temp, "values", *sTemp, "y"),
          linkByName(*temp, "name", *sTemp, "label"),
          linkByName(*time, "values", *sPot, "x"), linkByName(*pot, "values", *sPot, "y"),
          linkByName(*pot, "name", *sPot, "label"),
          linkByName(*time, "values", *sKin, "x"), linkByName(*kin, "values", *sKin, "y"),
          linkByName(*kin, "name", *sKin, "label"),
          linkByName(*temp, "values", *sHist, "y"),
          linkByName(*sTemp, "series", *pTemp, "s1"),
          linkByName(*sPot, "series", *pEnergy, "s1"), linkByName(*sKin, "series", *pEnergy, "s2"),
          linkByName(*sHist, "series", *pHist, "s1")})
        if (!err.empty()) return CommandResult::Error("demo wiring failed: " + err);

    // Show it in the Plot Lab UI (2D plot over the node graph) and run once so
    // the plots exist immediately.
    for (int i = 0; i < (int)s.uis.size(); ++i)
        if (s.uis[i].name == "Plot Lab") {
            s.activeUI = i;
            s.resetLayoutRequested = true;
        }
    s.PanelOpen("node_graph") = true;
    s.PanelOpen("plot_2d") = true;
    gs.Run(s);
    s.SelectPlot("Temperature");
    std::string msg = "Plots demo created: Load Table (md_demo.csv) -> Column x4 -> Series -> Plot 2D x3. "
                      "Pick a plot from the dropdown in the 2D Plot pane, or `plot list`.";
    if (!gs.lastRunOk) msg += "\nFirst run reported: " + gs.lastRunSummary;
    return CommandResult::Ok(msg);
}

// The graph-editing subcommands shared by `graph` and `canvas`:
//   add <type-id> [x y] | link <from-id> <out-pin> <to-id> <in-pin> | set <node-id> <param> <value>
CommandResult EditGraph(AppState& s, Graph& g, const CommandArgs& a) {
    if (a[0] == "add") {
        if (a.size() < 2) return CommandResult::Error("usage: <graph|canvas> add <type-id> [x y]");
        const float x = a.size() > 2 ? (float)std::atof(a[2].c_str()) : 100.0f;
        const float y = a.size() > 3 ? (float)std::atof(a[3].c_str()) : 100.0f;
        Node* n = g.AddNode(a[1], x, y);
        if (!n) return CommandResult::Error("unknown node type '" + a[1] + "' (see the add-node menu ids)");
        return CommandResult::Ok(fmt::format("Added node {} '{}'", n->id, n->title));
    }
    if (a[0] == "link") {
        if (a.size() < 5) return CommandResult::Error("usage: <graph|canvas> link <from-id> <out-pin> <to-id> <in-pin>");
        Node* from = g.FindNode((uint32_t)std::atoi(a[1].c_str()));
        Node* to = g.FindNode((uint32_t)std::atoi(a[3].c_str()));
        if (!from || !to) return CommandResult::Error("no such node");
        const int o = FindOutputPin(*from, a[2]), i = FindInputPin(*to, a[4]);
        if (o < 0) return CommandResult::Error(fmt::format("'{}' has no output '{}'", from->title, a[2]));
        if (i < 0) return CommandResult::Error(fmt::format("'{}' has no input '{}'", to->title, a[4]));
        std::string err;
        if (!g.AddLink(from->id, o, to->id, i, &err)) return CommandResult::Error(err);
        return CommandResult::Ok(fmt::format("{}.{} -> {}.{}", from->title, a[2], to->title, a[4]));
    }
    if (a[0] == "set") {
        if (a.size() < 4) return CommandResult::Error("usage: <graph|canvas> set <node-id> <param> <value>");
        Node* n = g.FindNode((uint32_t)std::atoi(a[1].c_str()));
        if (!n) return CommandResult::Error("no node with id " + a[1]);
        char* end = nullptr;
        const double num = std::strtod(a[3].c_str(), &end);
        n->params[a[2]] = (end && *end == 0 && end != a[3].c_str()) ? Value::F(num) : Value::S(a[3]);
        g.Touch();
        if (a[2] == "script" && n->typeId == "script.python") {
            if (std::string derr = DescribePythonNode(s, *n); !derr.empty())
                return CommandResult::Error("set, but describe failed: " + derr);
            return CommandResult::Ok(fmt::format("{}: script set ({} in / {} out)", n->title,
                                                 n->inputs.size(), n->outputs.size()));
        }
        return CommandResult::Ok(fmt::format("{}: {} = {}", n->title, a[2], a[3]));
    }
    return CommandResult::Error("expected add, link or set");
}

// `new [name]`, `save [name]`, `load <name>`, `list`: the named-graph
// operations of the Graph Canvas, reachable as `graph ...` and `canvas ...`.
CommandResult CanvasFileCommand(AppState& s, const CommandArgs& a) {
    GraphSystem& gs = s.GraphSys();
    CanvasGraph& cv = gs.canvas;
    if (a[0] == "new") {
        gs.NewCanvas(s, a.size() > 1 ? a[1] : "untitled");
        return CommandResult::Ok(fmt::format("Blank Graph Canvas '{}' opened: right-click the canvas to add nodes, "
                                             "Save keeps it as {}", cv.name, GraphPath(cv.name)));
    }
    if (a[0] == "save") {
        const bool ok = gs.SaveCanvas(a.size() > 1 ? a[1] : cv.name);
        return ok ? CommandResult::Ok(cv.lastIoMessage) : CommandResult::Error(cv.lastIoMessage);
    }
    if (a[0] == "load") {
        if (a.size() < 2) {
            const auto names = SavedGraphNames();
            return CommandResult::Error(names.empty() ? "usage: graph load <name> (no graphs saved yet)"
                                                      : "usage: graph load <name>; saved: " + fmt::format("{}", fmt::join(names, ", ")));
        }
        const bool ok = gs.LoadCanvas(s, a[1]);
        return ok ? CommandResult::Ok(cv.lastIoMessage) : CommandResult::Error(cv.lastIoMessage);
    }
    // list
    const auto names = SavedGraphNames();
    if (names.empty()) return CommandResult::Ok(fmt::format("no saved graphs in {}/ (use `graph save <name>`)", GraphsDir()));
    std::string out;
    for (const std::string& n : names) out += fmt::format("{}{}\n", n, n == cv.name ? "  (open)" : "");
    out.pop_back();
    return CommandResult::Ok(out);
}

}  // namespace

void RegisterGraphCommands(CommandRegistry& r) {
    r.Register({"graph", "graph <new|save|load|list|run|auto|demo|add|link|set|clear|python|show|hide|reset|panels> ...",
                "Node graph: `graph new [name]` opens a blank Graph Canvas to play in, `graph save [name]` / "
                "`graph load <name>` / `graph list` keep named graphs under graphs/. The rest drive the Node "
                "Graph panel: run it, build a demo, clear it, set the python interpreter, or open/reset a "
                "panel's graph (`graph show structure_view`).",
                "calculate", [](AppState& s, const CommandArgs& a) {
                    GraphSystem& gs = s.GraphSys();
                    const char* usage = "usage: graph <new|save|load|list|run|auto|demo|add|link|set|clear|python|show|hide|reset|panels> ...";
                    if (a.size() < 1) return CommandResult::Error(usage);
                    if (a[0] == "new" || a[0] == "save" || a[0] == "load" || a[0] == "list")
                        return CanvasFileCommand(s, a);
                    if (a[0] == "run") {
                        if (a.size() > 1) {   // graph run <panel-id>: force one panel graph
                            const std::string err = gs.RunPanel(s, a[1], true);
                            return err.empty() ? CommandResult::Ok(fmt::format("{} graph evaluated", a[1]))
                                               : CommandResult::Error(fmt::format("{} graph: {}", a[1], err));
                        }
                        const std::string summary = gs.Run(s);
                        return gs.lastRunOk ? CommandResult::Ok(summary) : CommandResult::Error(summary);
                    }
                    if (a[0] == "show" || a[0] == "hide") {
                        if (a.size() < 2) return CommandResult::Error(fmt::format("usage: graph {} <panel-id>", a[0]));
                        s.graphViewOpen[a[1]] = a[0] == "show";
                        return CommandResult::Ok(fmt::format("{} graph {}", a[1], a[0] == "show" ? "shown" : "hidden"));
                    }
                    if (a[0] == "reset") {
                        if (a.size() < 2) return CommandResult::Error("usage: graph reset <panel-id>");
                        gs.ResetPanel(s, a[1]);
                        return CommandResult::Ok(fmt::format("{} graph reset to its default", a[1]));
                    }
                    if (a[0] == "panels") {
                        std::string out;
                        for (const auto& [id, pg] : gs.panelGraphs)
                            out += fmt::format("{}: {} node{}, {} link{}{}\n", id, pg.graph.nodes.size(),
                                               pg.graph.nodes.size() == 1 ? "" : "s", pg.graph.links.size(),
                                               pg.graph.links.size() == 1 ? "" : "s",
                                               pg.lastError.empty() ? "" : " -- error: " + pg.lastError);
                        if (!out.empty()) out.pop_back();
                        return CommandResult::Ok(out.empty() ? "no panel graphs yet" : out);
                    }
                    if (a[0] == "demo") {
                        const std::string which = a.size() > 1 ? a[1] : "distance";
                        if (which == "distance") return BuildDistanceDemo(s);
                        if (which == "highlight") return BuildHighlightDemo(s);
                        if (which == "plots") return BuildPlotsDemo(s);
                        return CommandResult::Error("unknown demo (distance, highlight, plots)");
                    }
                    if (a[0] == "clear") {
                        gs.graph.Clear();
                        gs.store.Clear();
                        s.ClearPlots();
                        return CommandResult::Ok("Node graph cleared");
                    }
                    if (a[0] == "add" || a[0] == "link" || a[0] == "set") return EditGraph(s, gs.graph, a);
                    if (a[0] == "auto") {
                        if (a.size() > 1) {
                            if (a[1] == "off" || a[1] == "click") gs.autoRun = false;
                            else {
                                const double fps = std::atof(a[1].c_str());
                                if (fps <= 0) return CommandResult::Error("usage: graph auto <fps|off>");
                                gs.autoRun = true;
                                gs.autoRunFps = (float)fps;
                            }
                        }
                        return CommandResult::Ok(gs.autoRun
                                                     ? fmt::format("Auto-run at {:.1f} fps", gs.autoRunFps)
                                                     : "Auto-run off (run on click)");
                    }
                    if (a[0] == "python") {
                        if (a.size() > 1) gs.pythonExe = a[1];
                        return CommandResult::Ok(fmt::format("Python interpreter: {}{}", gs.pythonExe,
                                                             ScriptingAvailable() ? "" : " (unavailable on web)"));
                    }
                    return CommandResult::Error(usage);
                }});

    r.Register({"canvas", "canvas <run|auto|clear|info|add|link|set|new|save|load|list> ...",
                "The Graph Canvas (the graph `graph new` opens): run it, clear it, list its nodes, "
                "or edit it (`canvas add core.text 100 100`). new/save/load/list are the same as under `graph`.",
                "calculate", [](AppState& s, const CommandArgs& a) {
                    GraphSystem& gs = s.GraphSys();
                    CanvasGraph& cv = gs.canvas;
                    const char* usage = "usage: canvas <run|auto|clear|info|add|link|set|new|save|load|list> ...";
                    if (a.size() < 1) return CommandResult::Error(usage);
                    if (a[0] == "new" || a[0] == "save" || a[0] == "load" || a[0] == "list")
                        return CanvasFileCommand(s, a);
                    if (a[0] == "run") {
                        const std::string summary = gs.RunCanvas(s);
                        return cv.lastRunOk ? CommandResult::Ok(summary) : CommandResult::Error(summary);
                    }
                    if (a[0] == "clear") {
                        cv.graph.Clear();
                        return CommandResult::Ok("Graph Canvas cleared (the saved file is untouched)");
                    }
                    if (a[0] == "info") {
                        std::string out = fmt::format("'{}': {} node{}, {} link{}\n", cv.name, cv.graph.nodes.size(),
                                                      cv.graph.nodes.size() == 1 ? "" : "s", cv.graph.links.size(),
                                                      cv.graph.links.size() == 1 ? "" : "s");
                        for (const Node& n : cv.graph.nodes) {
                            const NodeTypeSpec* spec = NodeTypes().Find(n.typeId);
                            out += fmt::format("  {} '{}' [{}] {}\n", n.id, n.title, n.typeId,
                                               spec ? KindName(spec->kind) : "unknown type");
                        }
                        out.pop_back();
                        return CommandResult::Ok(out);
                    }
                    if (a[0] == "add" || a[0] == "link" || a[0] == "set") return EditGraph(s, cv.graph, a);
                    if (a[0] == "auto") {
                        if (a.size() > 1) {
                            if (a[1] == "off" || a[1] == "click") cv.autoRun = false;
                            else {
                                const double fps = std::atof(a[1].c_str());
                                if (fps <= 0) return CommandResult::Error("usage: canvas auto <fps|off>");
                                cv.autoRun = true;
                                gs.autoRunFps = (float)fps;
                            }
                        }
                        return CommandResult::Ok(cv.autoRun ? fmt::format("Canvas auto-run at {:.1f} fps", gs.autoRunFps)
                                                            : "Canvas auto-run off (run on click)");
                    }
                    return CommandResult::Error(usage);
                }});
}

}  // namespace graph
