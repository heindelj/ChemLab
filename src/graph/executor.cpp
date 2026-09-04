#include "graph/executor.h"

#include <chrono>
#include <deque>
#include <map>

#include <fmt/format.h>

#include "graph/graph.h"
#include "graph/node_registry.h"

namespace graph {

// ---------------------------------------------------------------------------
// KernelArgs helpers
// ---------------------------------------------------------------------------
double KernelArgs::FloatParam(const char* key, double fallback) const {
    if (!params) return fallback;
    auto it = params->find(key);
    double v = fallback;
    if (it == params->end() || !it->second.AsFloat(v)) return fallback;
    return v;
}

int64_t KernelArgs::IntParam(const char* key, int64_t fallback) const {
    if (!params) return fallback;
    auto it = params->find(key);
    int64_t v = fallback;
    if (it == params->end() || !it->second.AsInt(v)) return fallback;
    return v;
}

std::string KernelArgs::TextParam(const char* key, const std::string& fallback) const {
    if (!params) return fallback;
    auto it = params->find(key);
    if (it == params->end()) return fallback;
    const std::string* t = it->second.AsText();
    return t ? *t : fallback;
}

// ---------------------------------------------------------------------------
// Kernel table
// ---------------------------------------------------------------------------
void KernelTable::Register(const std::string& id, Kernel fn, const std::string& description) {
    for (KernelInfo& k : kernels)
        if (k.id == id) { k.fn = fn; k.description = description; return; }
    kernels.push_back({id, description, fn});
}

Kernel KernelTable::Find(const std::string& id) const {
    for (const KernelInfo& k : kernels)
        if (k.id == id) return k.fn;
    return nullptr;
}

KernelTable& Kernels() {
    static KernelTable table;
    static bool initialised = false;
    if (!initialised) {
        initialised = true;
        RegisterArrayKernels(table);
        RegisterChemKernels(table);
    }
    return table;
}

// ---------------------------------------------------------------------------
// Compile
// ---------------------------------------------------------------------------
std::string Compile(Graph& g, Program& out) {
    out.steps.clear();
    out.graphVersion = g.version;
    out.owner = &g;
    out.compileError.clear();

    // Dependency order (Kahn), same as Graph::Evaluate.
    std::map<uint32_t, int> indegree;
    for (const Node& n : g.nodes) indegree[n.id] = 0;
    for (const Link& l : g.links) ++indegree[l.toNode];
    std::deque<uint32_t> ready;
    for (const auto& [id, deg] : indegree)
        if (deg == 0) ready.push_back(id);
    std::vector<uint32_t> order;
    while (!ready.empty()) {
        const uint32_t id = ready.front();
        ready.pop_front();
        order.push_back(id);
        for (const Link& l : g.links)
            if (l.fromNode == id && --indegree[l.toNode] == 0) ready.push_back(l.toNode);
    }
    if (order.size() != g.nodes.size()) return out.compileError = "graph contains a cycle";

    for (const uint32_t id : order) {
        Node* node = g.FindNode(id);
        Step s;
        s.node = node;
        s.spec = NodeTypes().Find(node->typeId);
        if (!s.spec) return out.compileError = fmt::format("{}: unknown node type '{}'", node->title, node->typeId);
        if (!s.spec->kernel.empty()) {
            s.kernel = Kernels().Find(s.spec->kernel);
            if (!s.kernel)
                return out.compileError = fmt::format("{}: kernel '{}' is not in the kernel table", node->title, s.spec->kernel);
        } else if (!s.spec->evaluate) {
            return out.compileError = fmt::format("{}: node type has neither a kernel nor an evaluate function", node->title);
        }
        s.in.assign(node->inputs.size(), {nullptr, -1});
        for (size_t i = 0; i < node->inputs.size(); ++i)
            if (const Link* l = g.LinkInto(node->id, (int)i)) s.in[i] = {g.FindNode(l->fromNode), l->fromPin};
        node->outValues.resize(node->outputs.size());
        out.steps.push_back(std::move(s));
    }
    return "";
}

std::string EnsureCompiled(Graph& g, Program& p) {
    if (p.owner == &g && p.graphVersion == g.version && p.compileError.empty()) return "";
    return Compile(g, p);
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------
ExecStats Execute(AppState& state, Program& p) {
    using clock = std::chrono::steady_clock;
    ExecStats stats;
    if (!p.compileError.empty()) {
        stats.firstError = p.compileError;
        return stats;
    }
    const auto t0 = clock::now();
    std::vector<const Value*> ins;
    for (Step& s : p.steps) {
        Node& node = *s.node;
        const auto ts = clock::now();
        node.error.clear();
        node.skipped = false;
        for (Value& v : node.outValues) v = Value{};

        ins.assign(s.in.size(), nullptr);
        for (size_t i = 0; i < s.in.size() && node.error.empty() && !node.skipped; ++i) {
            const Node* up = s.in[i].first;
            if (!up) continue;
            if (up->skipped) node.skipped = true;
            else if (!up->error.empty()) node.error = fmt::format("upstream error in '{}'", up->title);
            else if (!up->outValues[(size_t)s.in[i].second].Empty()) ins[i] = &up->outValues[(size_t)s.in[i].second];
        }
        if (node.skipped) {
            ++stats.skipped;
            s.lastSeconds = 0.0;
            continue;
        }
        if (node.error.empty()) {
            if (s.kernel) {
                KernelArgs a;
                a.in = ins.data();
                a.nin = ins.size();
                a.out = node.outValues.data();
                a.nout = node.outValues.size();
                a.params = &node.params;
                node.error = s.kernel(a);
                if (node.error.empty() && a.skip) node.skipped = true;
            } else {
                node.error = s.spec->evaluate(state, node, ins, node.outValues);
            }
        }
        s.lastSeconds = std::chrono::duration<double>(clock::now() - ts).count();
        if (!node.error.empty()) {
            ++stats.failed;
            if (stats.firstError.empty()) stats.firstError = fmt::format("{}: {}", node.title, node.error);
        } else if (node.skipped) {
            ++stats.skipped;   // a Gate that closed: it ran, but counts as the start of the skipped region
        } else {
            ++stats.ran;
        }
    }
    stats.seconds = std::chrono::duration<double>(clock::now() - t0).count();
    return stats;
}

}  // namespace graph
