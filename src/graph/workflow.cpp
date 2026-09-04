#include "graph/workflow.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "app/actions.h"
#include "app/app_state.h"
#include "app/commands.h"
#include "core/chemical_data.h"
#include "core/element.h"
#include "core/neighbor_list.h"
#include "graph/graph_io.h"
#include "graph/graph_system.h"

using json = nlohmann::json;

namespace graph {

// ---------------------------------------------------------------------------
// Triggers
// ---------------------------------------------------------------------------
const char* TriggerName(Trigger t) {
    switch (t) {
        case Trigger::Manual: return "manual";
        case Trigger::FrameChange: return "frame";
    }
    return "manual";
}

bool TriggerFromName(const std::string& s, Trigger& out) {
    if (s == "manual" || s == "off") { out = Trigger::Manual; return true; }
    if (s == "frame" || s == "frame-change" || s == "active-frame") { out = Trigger::FrameChange; return true; }
    return false;
}

const char* TriggerDescription(Trigger t) {
    switch (t) {
        case Trigger::Manual: return "run on demand only";
        case Trigger::FrameChange: return "run when the active frame changes";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
namespace {
std::string gWorkflowsDir;
}
std::string WorkflowsDir() { return gWorkflowsDir.empty() ? "workflows" : gWorkflowsDir; }
void SetWorkflowsDir(const std::string& dir) { gWorkflowsDir = dir; }
std::string WorkflowPath(const std::string& name) { return WorkflowsDir() + "/" + name + ".json"; }

bool SaveWorkflow(const Workflow& w, std::string& err) {
    std::error_code ec;
    std::filesystem::create_directories(WorkflowsDir(), ec);
    json j = GraphToJson(w.graph);
    j["workflow"] = {{"name", w.name}, {"description", w.description}, {"trigger", TriggerName(w.trigger)}, {"enabled", w.enabled}};
    const std::string path = WorkflowPath(w.name);
    std::ofstream out(path);
    if (!out) { err = "cannot write " + path; return false; }
    out << j.dump(2) << '\n';
    if (!out) { err = "write failed for " + path; return false; }
    return true;
}

bool LoadWorkflowFile(const std::string& path, Workflow& out, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open " + path; return false; }
    const json j = json::parse(in, nullptr, false);
    if (j.is_discarded()) { err = "invalid JSON in " + path; return false; }
    if (!GraphFromJson(j, out.graph, err)) return false;
    out.name = std::filesystem::path(path).stem().string();
    out.builtin = false;
    if (j.contains("workflow") && j["workflow"].is_object()) {
        const json& w = j["workflow"];
        out.description = w.value("description", "");
        out.enabled = w.value("enabled", true);
        Trigger t;
        if (TriggerFromName(w.value("trigger", "manual"), t)) out.trigger = t;
    }
    return true;
}

std::vector<Workflow> LoadUserWorkflows(std::vector<std::string>& errors) {
    std::vector<Workflow> out;
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(WorkflowsDir(), ec))
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const auto& f : files) {
        Workflow w;
        std::string err;
        if (LoadWorkflowFile(f.string(), w, err)) out.push_back(std::move(w));
        else errors.push_back(fmt::format("{}: {}", f.filename().string(), err));
    }
    return out;
}

// ---------------------------------------------------------------------------
// The built-in covalent-bonds workflow
// ---------------------------------------------------------------------------
namespace {

struct Wiring {
    Graph& g;
    std::string err;
    Node* Add(const char* type, const char* title, float x, float y) {
        Node* n = g.AddNode(type, x, y);
        if (!n) { if (err.empty()) err = fmt::format("node type '{}' missing", type); return nullptr; }
        n->title = title;
        return n;
    }
    void Link(Node* from, const char* out, Node* to, const char* in) {
        if (!from || !to) return;
        const int o = FindOutputPin(*from, out), i = FindInputPin(*to, in);
        if (o < 0 || i < 0) { if (err.empty()) err = fmt::format("missing pin {}.{} -> {}.{}", from->title, out, to->title, in); return; }
        std::string lerr;
        if (!g.AddLink(from->id, o, to->id, i, &lerr) && err.empty()) err = lerr;
    }
};

}  // namespace

Workflow MakeCovalentBondsWorkflow() {
    Workflow w;
    w.name = "covalent-bonds";
    w.description = "Detect covalent bonds of the active frame from covalent radii (cell-list neighbour search, "
                    "vectorised compare); skipped when the frame already has a 'bonds' topology.";
    w.trigger = Trigger::FrameChange;
    w.builtin = true;
    Wiring wire{w.graph};

    // Source and the cache gate.
    Node* chem = wire.Add("core.chemical_data", "Active Frame", 40, 220);
    Node* has = wire.Add("chem.has_topology", "Has Bonds?", 300, 100);
    Node* gate = wire.Add("flow.gate", "If Missing", 540, 220);
    // Per-atom data and the search cutoff.
    Node* rcov = wire.Add("chem.covalent_radii", "Covalent Radii", 800, 60);
    Node* tol = wire.Add("core.float", "Tolerance", 800, 460);
    Node* twice = wire.Add("scalar.math", "2 * max r", 1060, 60);
    Node* cutoff = wire.Add("scalar.math", "Search Cutoff", 1320, 60);
    Node* nl = wire.Add("chem.neighbor_list", "Neighbor List", 1580, 220);
    // Vectorised per-pair criterion: d < r_i + r_j + tol.
    Node* ri = wire.Add("array.gather", "r_i", 1860, 40);
    Node* rj = wire.Add("array.gather", "r_j", 1860, 200);
    Node* rsum = wire.Add("array.math", "r_i + r_j", 2120, 120);
    Node* rcut = wire.Add("array.math", "+ Tolerance", 2380, 120);
    Node* cmp = wire.Add("array.compare", "d < cutoff", 2640, 220);
    Node* fi = wire.Add("array.filter", "Bonded i", 2900, 120);
    Node* fj = wire.Add("array.filter", "Bonded j", 2900, 320);
    Node* apply = wire.Add("core.apply_topology", "Store Bonds", 3160, 220);
    if (!wire.err.empty()) { w.description = "BROKEN: " + wire.err; return w; }

    has->params["name"] = Value::S("bonds");
    tol->params["value"] = Value::F(0.4);
    twice->params["op"] = Value::S("mul");
    twice->params["b"] = Value::F(2.0);
    cutoff->params["op"] = Value::S("add");
    rsum->params["op"] = Value::S("add");
    rcut->params["op"] = Value::S("add");
    cmp->params["op"] = Value::S("lt");
    apply->params["name"] = Value::S("bonds");

    wire.Link(chem, "chem", has, "chem");
    wire.Link(has, "missing", gate, "pass");
    wire.Link(chem, "chem", gate, "value");
    wire.Link(gate, "value", rcov, "chem");
    wire.Link(rcov, "max", twice, "a");
    wire.Link(twice, "value", cutoff, "a");
    wire.Link(tol, "value", cutoff, "b");
    wire.Link(gate, "value", nl, "chem");
    wire.Link(cutoff, "value", nl, "cutoff");
    wire.Link(rcov, "radii", ri, "values");
    wire.Link(nl, "i", ri, "indices");
    wire.Link(rcov, "radii", rj, "values");
    wire.Link(nl, "j", rj, "indices");
    wire.Link(ri, "values", rsum, "a");
    wire.Link(rj, "values", rsum, "b");
    wire.Link(rsum, "values", rcut, "a");
    wire.Link(tol, "value", rcut, "b");
    wire.Link(nl, "distances", cmp, "a");
    wire.Link(rcut, "values", cmp, "b");
    wire.Link(nl, "i", fi, "values");
    wire.Link(cmp, "mask", fi, "mask");
    wire.Link(nl, "j", fj, "values");
    wire.Link(cmp, "mask", fj, "mask");
    wire.Link(fi, "values", apply, "i");
    wire.Link(fj, "values", apply, "j");
    if (!wire.err.empty()) w.description = "BROKEN: " + wire.err;
    return w;
}

std::vector<Workflow> BuiltinWorkflows() {
    std::vector<Workflow> out;
    out.push_back(MakeCovalentBondsWorkflow());
    return out;
}

// ---------------------------------------------------------------------------
// GraphSystem
// ---------------------------------------------------------------------------
void GraphSystem::LoadWorkflows(AppState& state) {
    workflows = BuiltinWorkflows();
    std::vector<std::string> errors;
    for (Workflow& w : LoadUserWorkflows(errors)) {
        const int existing = FindWorkflow(w.name);   // a user workflow shadows a built-in of the same name
        if (existing >= 0) workflows[(size_t)existing] = std::move(w);
        else workflows.push_back(std::move(w));
    }
    for (const std::string& e : errors) Log(state, LogLevel::Warning, WorkflowsDir() + "/: " + e);
}

int GraphSystem::FindWorkflow(const std::string& name) const {
    for (size_t i = 0; i < workflows.size(); ++i)
        if (workflows[i].name == name) return (int)i;
    return -1;
}

std::string GraphSystem::RunWorkflow(AppState& state, Workflow& w) {
    ++w.runCount;
    w.lastRunSkipped = false;
    if (const std::string err = EnsureCompiled(w.graph, w.graph.program); !err.empty()) {
        w.lastRunOk = false;
        w.lastRunSummary = "compile: " + err;
        return w.lastRunSummary;
    }
    w.lastStats = Execute(state, w.graph.program);
    w.graph.lastStats = w.lastStats;
    w.lastRunOk = w.lastStats.failed == 0;
    if (!w.lastRunOk) {
        std::string s;
        for (const Node& n : w.graph.nodes)
            if (!n.error.empty()) s += fmt::format("{}: {}\n", n.title, n.error);
        if (!s.empty()) s.pop_back();
        w.lastRunSummary = s;
    } else if (w.lastStats.skipped > 0 && w.lastStats.skipped >= w.lastStats.ran) {
        // Most of the graph sat behind a closed gate: nothing to do.
        w.lastRunSkipped = true;
        w.lastRunSummary = fmt::format("skipped ({} node{} behind a closed gate) in {:.3f} ms", w.lastStats.skipped,
                                       w.lastStats.skipped == 1 ? "" : "s", w.lastStats.seconds * 1e3);
    } else {
        w.lastRunSummary = fmt::format("ran {} node{}{} in {:.3f} ms", w.lastStats.ran, w.lastStats.ran == 1 ? "" : "s",
                                       w.lastStats.skipped ? fmt::format(", skipped {}", w.lastStats.skipped) : "",
                                       w.lastStats.seconds * 1e3);
    }
    return w.lastRunSummary;
}

bool GraphSystem::RemoveWorkflow(int index, std::string& err) {
    if (index < 0 || index >= (int)workflows.size()) { err = "no such workflow"; return false; }
    if (workflows[(size_t)index].builtin) { err = "built-in workflows cannot be removed"; return false; }
    std::error_code ec;
    std::filesystem::remove(WorkflowPath(workflows[(size_t)index].name), ec);
    workflows.erase(workflows.begin() + index);
    return true;
}

// ---------------------------------------------------------------------------
// Triggers: polled once per frame
// ---------------------------------------------------------------------------
void UpdateWorkflowTriggers(AppState& state) {
    // Fingerprint of "which frame is active": structure, frame index, its
    // atom count and the trajectory's data version (hot reload).
    struct Stamp { int structure = -2; int frame = -1; uint32_t natoms = 0; uint64_t dataVersion = ~0ull; bool operator==(const Stamp&) const = default; };
    static Stamp last;
    Stamp now;
    now.structure = state.activeStructure;
    if (const Structure* s = state.ActiveStructure()) {
        now.frame = s->activeFrame;
        now.dataVersion = s->frames.dataVersion;
        if (const ChemicalData* c = state.ActiveChem()) now.natoms = c->natoms;
    }
    if (now == last) return;
    last = now;
    if (!state.ActiveChem()) return;
    GraphSystem& gs = state.GraphSys();
    for (Workflow& w : gs.workflows) {
        if (!w.enabled || w.trigger != Trigger::FrameChange) continue;
        gs.RunWorkflow(state, w);
        if (!w.lastRunOk) Log(state, LogLevel::Warning, fmt::format("workflow '{}': {}", w.name, w.lastRunSummary));
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
namespace {

std::string StepReport(const Workflow& w) {
    std::string out;
    for (const Step& s : w.graph.program.steps) {
        const Node& n = *s.node;
        out += fmt::format("  {:<16} {:>9.3f} ms{}{}\n", n.title, s.lastSeconds * 1e3, n.skipped ? "  (skipped)" : "",
                           n.error.empty() ? "" : "  error: " + n.error);
    }
    if (!out.empty()) out.pop_back();
    return out;
}

// Time the covalent-bond workflow against PerceiveBonds on the active frame.
// The frame's bonds are removed before every workflow run so the gate opens
// (the point is to measure the work, not the cache) and restored afterwards.
CommandResult Bench(AppState& s, const CommandArgs& a) {
    GraphSystem& gs = s.GraphSys();
    std::string name = "covalent-bonds";
    int iterations = 5;
    for (size_t k = 1; k < a.size(); ++k) {
        char* end = nullptr;
        const long v = std::strtol(a[k].c_str(), &end, 10);
        if (end && *end == 0 && end != a[k].c_str()) iterations = (int)std::clamp(v, 1L, 1000L);
        else name = a[k];
    }
    const int wi = gs.FindWorkflow(name);
    if (wi < 0) return CommandResult::Error(fmt::format("no workflow '{}'", name));
    Workflow& w = gs.workflows[(size_t)wi];
    ChemicalData* c = s.ActiveChem();
    if (!c) return CommandResult::Error("no structure loaded");
    const std::vector<Topology> saved = c->topologies;
    auto dropBonds = [&] {
        c->topologies.erase(std::remove_if(c->topologies.begin(), c->topologies.end(),
                                           [](const Topology& t) { return t.name == "bonds"; }),
                            c->topologies.end());
    };
    using clock = std::chrono::steady_clock;
    auto ms = [](clock::time_point t0, clock::time_point t1) { return std::chrono::duration<double, std::milli>(t1 - t0).count(); };

    // 1. the workflow (executor), gate open
    std::vector<double> wfTimes;
    std::vector<std::pair<int32_t, int32_t>> wfPairs;
    for (int it = 0; it < iterations; ++it) {
        dropBonds();
        const auto t0 = clock::now();
        gs.RunWorkflow(s, w);
        wfTimes.push_back(ms(t0, clock::now()));
        if (!w.lastRunOk) { c->topologies = saved; return CommandResult::Error("workflow failed: " + w.lastRunSummary); }
        if (w.lastRunSkipped) { c->topologies = saved; return CommandResult::Error("workflow skipped itself: does it start with Has Topology -> Gate?"); }
    }
    if (const Topology* t = c->FindTopology("bonds")) wfPairs = t->pairs;
    const std::string steps = StepReport(w);
    // 2. the workflow with the gate closed (bonds present): the cached path
    std::vector<double> gateTimes;
    for (int it = 0; it < iterations; ++it) {
        const auto t0 = clock::now();
        gs.RunWorkflow(s, w);
        gateTimes.push_back(ms(t0, clock::now()));
    }
    // 3. PerceiveBonds, the existing path
    std::vector<double> pbTimes;
    for (int it = 0; it < iterations; ++it) {
        dropBonds();
        const auto t0 = clock::now();
        PerceiveBonds(*c, s.calc.bondTolerance);
        pbTimes.push_back(ms(t0, clock::now()));
    }
    std::vector<std::pair<int32_t, int32_t>> pbPairs;
    if (const Topology* t = c->FindTopology("bonds")) pbPairs = t->pairs;
    // 4. the neighbour-list kernel alone, for reference
    double maxR = 0.0;
    for (uint32_t i = 0; i < c->natoms && i < c->Z.size(); ++i) maxR = std::max(maxR, (double)CovalentRadius(c->Z[i]));
    std::vector<double> nlTimes;
    size_t nlPairs = 0;
    for (int it = 0; it < iterations; ++it) {
        const auto t0 = clock::now();
        const NeighborList nl = BuildNeighborList(c->R.data(), c->natoms, 2.0 * maxR + s.calc.bondTolerance);
        nlTimes.push_back(ms(t0, clock::now()));
        nlPairs = nl.Count();
    }
    c->topologies = saved;
    MarkGeometryChanged(s);

    auto stat = [](std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return fmt::format("best {:.3f} ms, median {:.3f} ms", v.front(), v[v.size() / 2]);
    };
    std::sort(wfPairs.begin(), wfPairs.end());
    std::sort(pbPairs.begin(), pbPairs.end());
    const bool same = wfPairs == pbPairs;
    const double tolNode = [&] {
        for (const Node& n : w.graph.nodes)
            if (n.typeId == "core.float" && n.title == "Tolerance") { double v = 0.4; if (auto it = n.params.find("value"); it != n.params.end()) it->second.AsFloat(v); return v; }
        return 0.4;
    }();
    std::string out = fmt::format(
        "Benchmark: {} atoms, {} iteration{}\n"
        "  workflow '{}' (executor, gate open):   {}  -> {} bonds\n"
        "  workflow, gate closed (bonds cached):  {}\n"
        "  PerceiveBonds (current C++ path):      {}  -> {} bonds\n"
        "  neighbour-list kernel alone:           {}  ({} candidate pairs, cutoff {:.2f} A)\n"
        "  results {}{}\n"
        "Per-node time of the last open-gate run:\n{}",
        c->natoms, iterations, iterations == 1 ? "" : "s", w.name, stat(wfTimes), wfPairs.size(), stat(gateTimes),
        stat(pbTimes), pbPairs.size(), stat(nlTimes), nlPairs, 2.0 * maxR + s.calc.bondTolerance,
        same ? "identical" : "DIFFER",
        (float)tolNode != s.calc.bondTolerance
            ? fmt::format(" (workflow tolerance {:.2f} A vs calc.bondTolerance {:.2f} A)", tolNode, s.calc.bondTolerance)
            : "",
        steps);
    return same ? CommandResult::Ok(out) : CommandResult::Error(out);
}

}  // namespace

void RegisterWorkflowCommands(CommandRegistry& r) {
    r.Register({"workflow", "workflow [list|run <name>|graph <name>|trigger <name> <manual|frame>|enable <name> <on|off>|new <name>|save <name>|delete <name>|kernels|bench [name] [n]]",
                "Workflows: node graphs run by the executor. `workflow list` shows built-in and user workflows with their "
                "triggers, `workflow run <name>` runs one now, `workflow graph <name>` opens its graph, `workflow trigger "
                "<name> frame|manual` sets when it runs, `workflow new <name>` makes an empty user workflow, `workflow save` "
                "writes it to workflows/<name>.json. `workflow kernels` lists the native kernel table and `workflow bench` "
                "times covalent bond detection through the graph against PerceiveBonds.",
                "calculate", [](AppState& s, const CommandArgs& a) {
                    GraphSystem& gs = s.GraphSys();
                    auto find = [&](const std::string& name, Workflow*& w) -> CommandResult {
                        const int i = gs.FindWorkflow(name);
                        if (i < 0) return CommandResult::Error(fmt::format("no workflow '{}' (try `workflow list`)", name));
                        w = &gs.workflows[(size_t)i];
                        return CommandResult::Ok();
                    };
                    if (a.size() < 1 || a[0] == "list") {
                        std::string out = "Workflows:";
                        for (const Workflow& w : gs.workflows)
                            out += fmt::format("\n  {:<18} {:<7} {}{}{} -- {}", w.name, TriggerName(w.trigger),
                                               w.builtin ? "built-in" : "user", w.enabled ? "" : ", disabled",
                                               w.runCount ? fmt::format(", last: {}", w.lastRunSummary) : "", w.description);
                        return CommandResult::Ok(out);
                    }
                    if (a[0] == "kernels") {
                        std::string out = "Native kernels:";
                        for (const KernelInfo& k : Kernels().All()) out += fmt::format("\n  {:<22} {}", k.id, k.description);
                        return CommandResult::Ok(out);
                    }
                    if (a[0] == "bench") return Bench(s, a);
                    if (a.size() < 2) return CommandResult::Error(fmt::format("usage: workflow {} <name> ...", a[0]));
                    if (a[0] == "new") {
                        if (gs.FindWorkflow(a[1]) >= 0) return CommandResult::Error(fmt::format("a workflow called '{}' already exists", a[1]));
                        Workflow w;
                        w.name = a[1];
                        w.graphOpen = true;
                        gs.workflows.push_back(std::move(w));
                        return CommandResult::Ok(fmt::format("Workflow '{}' created (empty): right-click its graph to add nodes, `workflow save {}` keeps it", a[1], a[1]));
                    }
                    Workflow* w = nullptr;
                    if (CommandResult r = find(a[1], w); !r.ok) return r;
                    if (a[0] == "run") {
                        const std::string summary = gs.RunWorkflow(s, *w);
                        return w->lastRunOk ? CommandResult::Ok(fmt::format("{}: {}", w->name, summary))
                                            : CommandResult::Error(fmt::format("{}: {}", w->name, summary));
                    }
                    if (a[0] == "graph") {
                        w->graphOpen = true;
                        return CommandResult::Ok(fmt::format("Workflow graph '{}' opened", w->name));
                    }
                    if (a[0] == "steps") {
                        if (!w->graph.program.Compiled()) return CommandResult::Error("not compiled yet: run it first");
                        return CommandResult::Ok(StepReport(*w));
                    }
                    if (a[0] == "trigger") {
                        if (a.size() < 3) return CommandResult::Ok(fmt::format("{}: trigger {} ({})", w->name, TriggerName(w->trigger), TriggerDescription(w->trigger)));
                        Trigger t;
                        if (!TriggerFromName(a[2], t)) return CommandResult::Error("trigger must be manual or frame");
                        w->trigger = t;
                        return CommandResult::Ok(fmt::format("{}: trigger {} ({})", w->name, TriggerName(t), TriggerDescription(t)));
                    }
                    if (a[0] == "enable") {
                        if (a.size() > 2) w->enabled = a[2] == "on" || a[2] == "true" || a[2] == "1";
                        return CommandResult::Ok(fmt::format("{}: {}", w->name, w->enabled ? "enabled" : "disabled"));
                    }
                    if (a[0] == "save") {
                        if (a.size() > 2) w->name = a[2];
                        std::string err;
                        if (!SaveWorkflow(*w, err)) return CommandResult::Error(err);
                        w->builtin = false;
                        w->lastIoMessage = "saved to " + WorkflowPath(w->name);
                        return CommandResult::Ok(fmt::format("Workflow '{}' saved to {}", w->name, WorkflowPath(w->name)));
                    }
                    if (a[0] == "delete") {
                        std::string err;
                        if (!gs.RemoveWorkflow(gs.FindWorkflow(a[1]), err)) return CommandResult::Error(err);
                        return CommandResult::Ok(fmt::format("Workflow '{}' removed", a[1]));
                    }
                    return CommandResult::Error("usage: workflow [list|run|graph|steps|trigger|enable|new|save|delete|kernels|bench]");
                }});
}

}  // namespace graph
