#include "graph/graph_io.h"

#include <algorithm>
#include <fstream>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "graph/graph.h"

using json = nlohmann::json;

namespace graph {

namespace {

constexpr int kFormatVersion = 1;

json PinsToJson(const std::vector<PinSpec>& pins) {
    json arr = json::array();
    for (const PinSpec& p : pins) arr.push_back({{"name", p.name}, {"type", TypeName(p.type)}});
    return arr;
}

bool PinsFromJson(const json& j, std::vector<PinSpec>& out, std::string& err) {
    out.clear();
    if (!j.is_array()) { err = "pins must be an array"; return false; }
    for (const auto& e : j) {
        PinSpec p;
        p.name = e.value("name", "");
        const std::string type = e.value("type", "any");
        if (p.name.empty()) { err = "pin without a name"; return false; }
        if (!TypeFromName(type, p.type)) { err = "unknown pin type '" + type + "'"; return false; }
        out.push_back(std::move(p));
    }
    return true;
}

}  // namespace

json GraphToJson(const Graph& g) {
    json nodes = json::array();
    for (const Node& n : g.nodes) {
        json params = json::object();
        for (const auto& [key, v] : n.params) {
            if (v.Empty()) continue;
            if (!key.empty() && key[0] == '_') continue;   // transient/derived ("_label")
            params[key] = {{"type", TypeName(v.Type())}, {"value", ValueToJson(v)}};
        }
        nodes.push_back({{"id", n.id},
                         {"type", n.typeId},
                         {"title", n.title},
                         {"pos", {n.posX, n.posY}},
                         {"inputs", PinsToJson(n.inputs)},
                         {"outputs", PinsToJson(n.outputs)},
                         {"params", params}});
    }
    json links = json::array();
    for (const Link& l : g.links) {
        const Node* from = g.FindNode(l.fromNode);
        const Node* to = g.FindNode(l.toNode);
        if (!from || !to) continue;
        links.push_back({{"id", l.id},
                         {"from", l.fromNode},
                         {"out", l.fromPin < (int)from->outputs.size() ? from->outputs[l.fromPin].name : ""},
                         {"to", l.toNode},
                         {"in", l.toPin < (int)to->inputs.size() ? to->inputs[l.toPin].name : ""}});
    }
    return json{{"format", "chemlab-graph"}, {"version", kFormatVersion}, {"nodes", nodes}, {"links", links}};
}

bool GraphFromJson(const json& j, Graph& g, std::string& err) {
    g.Clear();
    if (!j.is_object() || !j.contains("nodes") || !j["nodes"].is_array()) {
        err = "not a chemlab graph document (no \"nodes\" array)";
        return false;
    }
    if (j.value("version", kFormatVersion) > kFormatVersion) {
        err = fmt::format("graph format version {} is newer than this build understands ({})",
                          j.value("version", 0), kFormatVersion);
        return false;
    }
    for (const auto& jn : j["nodes"]) {
        Node n;
        n.id = jn.value("id", 0u);
        n.typeId = jn.value("type", "");
        if (n.id == 0 || n.typeId.empty()) { err = "node without an id or type"; g.Clear(); return false; }
        if (g.FindNode(n.id)) { err = fmt::format("duplicate node id {}", n.id); g.Clear(); return false; }
        const NodeTypeSpec* spec = NodeTypes().Find(n.typeId);
        n.title = jn.value("title", spec ? fmt::format("{} {}", spec->name, n.id) : n.typeId);
        if (jn.contains("pos") && jn["pos"].is_array() && jn["pos"].size() == 2) {
            n.posX = jn["pos"][0].get<float>();
            n.posY = jn["pos"][1].get<float>();
        }
        n.posDirty = true;
        // Saved pins win (script nodes discover theirs per instance); fall
        // back to the type's static pins for older/hand-written files.
        if (jn.contains("inputs")) {
            if (!PinsFromJson(jn["inputs"], n.inputs, err)) { g.Clear(); return false; }
        } else if (spec) {
            n.inputs = spec->inputs;
        }
        if (jn.contains("outputs")) {
            if (!PinsFromJson(jn["outputs"], n.outputs, err)) { g.Clear(); return false; }
        } else if (spec) {
            n.outputs = spec->outputs;
        }
        n.outValues.assign(n.outputs.size(), Value{});
        if (jn.contains("params") && jn["params"].is_object()) {
            for (const auto& [key, jp] : jn["params"].items()) {
                Value v;
                std::string verr;
                ValueType t = ValueType::Any;
                const json& raw = jp.is_object() && jp.contains("value") ? jp["value"] : jp;
                if (jp.is_object() && jp.contains("type")) TypeFromName(jp["type"].get<std::string>(), t);
                if (!ValueFromJson(raw, t, v, verr)) {
                    err = fmt::format("node {} param '{}': {}", n.id, key, verr);
                    g.Clear();
                    return false;
                }
                n.params[key] = std::move(v);
            }
        }
        g.nextNodeId = std::max(g.nextNodeId, n.id + 1);
        g.nodes.push_back(std::move(n));
    }
    if (j.contains("links") && j["links"].is_array()) {
        for (const auto& jl : j["links"]) {
            const uint32_t fromId = jl.value("from", 0u), toId = jl.value("to", 0u);
            const Node* from = g.FindNode(fromId);
            const Node* to = g.FindNode(toId);
            if (!from || !to) { err = fmt::format("link between unknown nodes {} -> {}", fromId, toId); g.Clear(); return false; }
            const int o = FindOutputPin(*from, jl.value("out", ""));
            const int i = FindInputPin(*to, jl.value("in", ""));
            if (o < 0 || i < 0) {
                err = fmt::format("link {}.{} -> {}.{}: no such pin", from->title, jl.value("out", ""), to->title,
                                  jl.value("in", ""));
                g.Clear();
                return false;
            }
            std::string lerr;
            if (!g.AddLink(fromId, o, toId, i, &lerr)) { err = "link rejected: " + lerr; g.Clear(); return false; }
            const uint32_t savedId = jl.value("id", 0u);
            if (savedId) {   // keep the saved link id so the editor's selection state stays meaningful
                g.links.back().id = savedId;
                g.nextLinkId = std::max(g.nextLinkId, savedId + 1);
            }
        }
    }
    g.Touch();
    return true;
}

bool SaveGraph(const Graph& g, const std::string& path, std::string& err) {
    std::ofstream out(path);
    if (!out) { err = "cannot write " + path; return false; }
    out << GraphToJson(g).dump(2) << '\n';
    if (!out) { err = "write failed for " + path; return false; }
    return true;
}

bool LoadGraph(const std::string& path, Graph& g, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open " + path; return false; }
    const json j = json::parse(in, nullptr, false);
    if (j.is_discarded()) { err = "invalid JSON in " + path; return false; }
    return GraphFromJson(j, g, err);
}

}  // namespace graph
