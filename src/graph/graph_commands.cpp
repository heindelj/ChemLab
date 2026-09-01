// GraphSystem::Run plus the `graph` command family, so the node graph is fully
// drivable from the command bar (and therefore from --run smoke tests).

#include <fmt/format.h>

#include "app/app_state.h"
#include "app/commands.h"
#include "core/molecule.h"
#include "graph/graph_system.h"
#include "graph/py_runner.h"

namespace graph {

std::string GraphSystem::Run(AppState& state) {
    ++runCount;
    const std::string err = graph.Evaluate(state, store);
    std::string summary;
    for (const Node& n : graph.nodes) {
        if (!n.error.empty()) {
            summary += fmt::format("{}: error: {}\n", n.title, n.error);
            continue;
        }
        for (size_t k = 0; k < n.outputs.size(); ++k)
            summary += fmt::format("{}.{} = {}\n", n.title, n.outputs[k].name, n.outValues[k].Preview());
    }
    if (!summary.empty() && summary.back() == '\n') summary.pop_back();
    if (summary.empty()) summary = "graph is empty";
    lastRunOk = err.empty();
    lastRunSummary = summary;
    return summary;
}

namespace {

CommandResult BuildDemoGraph(AppState& s) {
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

}  // namespace

void RegisterGraphCommands(CommandRegistry& r) {
    r.Register({"graph", "graph <run|demo|clear|python [exe]>",
                "Node graph: run it, build the demo graph, clear it, or set the python interpreter.",
                "calculate", [](AppState& s, const CommandArgs& a) {
                    GraphSystem& gs = s.GraphSys();
                    if (a.size() < 1) return CommandResult::Error("usage: graph <run|demo|clear|python [exe]>");
                    if (a[0] == "run") {
                        const std::string summary = gs.Run(s);
                        return gs.lastRunOk ? CommandResult::Ok(summary) : CommandResult::Error(summary);
                    }
                    if (a[0] == "demo") return BuildDemoGraph(s);
                    if (a[0] == "clear") {
                        gs.graph.Clear();
                        gs.store.Clear();
                        return CommandResult::Ok("Node graph cleared");
                    }
                    if (a[0] == "python") {
                        if (a.size() > 1) gs.pythonExe = a[1];
                        return CommandResult::Ok(fmt::format("Python interpreter: {}{}", gs.pythonExe,
                                                             ScriptingAvailable() ? "" : " (unavailable on web)"));
                    }
                    return CommandResult::Error("usage: graph <run|demo|clear|python [exe]>");
                }});
}

}  // namespace graph
