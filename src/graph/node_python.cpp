// The Python Script node: wraps an external script speaking the JSON protocol
// in py_runner.h. Pins are discovered by running `<script> --describe`, so a
// plain .py file is a complete third-party node.

#include <filesystem>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "imgui.h"

#include "app/app_state.h"
#include "graph/graph_system.h"
#include "graph/node_registry.h"
#include "graph/py_runner.h"

#if !defined(__EMSCRIPTEN__)
#include "portable-file-dialogs.h"
#endif

namespace graph {

namespace {

using nlohmann::json;

std::string TextParam(const Node& n, const std::string& key) {
    auto it = n.params.find(key);
    if (it == n.params.end()) return "";
    const std::string* s = it->second.AsText();
    return s ? *s : "";
}

bool ParsePins(const json& arr, std::vector<PinSpec>& out, std::string& err) {
    out.clear();
    if (arr.is_null()) return true;
    if (!arr.is_array()) { err = "'inputs'/'outputs' must be arrays"; return false; }
    for (const auto& e : arr) {
        if (!e.is_object() || !e.contains("name") || !e["name"].is_string()) {
            err = "each pin needs a \"name\"";
            return false;
        }
        PinSpec p;
        p.name = e["name"].get<std::string>();
        if (e.contains("type")) {
            if (!e["type"].is_string() || !TypeFromName(e["type"].get<std::string>(), p.type)) {
                err = fmt::format("pin '{}': unknown type", p.name);
                return false;
            }
        }
        out.push_back(std::move(p));
    }
    return true;
}

std::string EvalPython(AppState& s, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    const std::string script = TextParam(n, "script");
    if (script.empty()) return "no script set";
    if (n.inputs.empty() && n.outputs.empty()) return "script not described yet (press Reload)";

    json req;
    req["inputs"] = json::object();
    for (size_t i = 0; i < n.inputs.size(); ++i) {
        if (!in[i]) return fmt::format("input '{}' not connected", n.inputs[i].name);
        req["inputs"][n.inputs[i].name] = ValueToJson(*in[i]);
    }

    RunResult r = RunScript(s.GraphSys().pythonExe, script, "", req.dump());
    if (!r.ok) return r.error;

    json resp = json::parse(r.output, nullptr, false);
    if (resp.is_discarded()) return "script did not print valid JSON";
    if (resp.contains("error")) return resp["error"].is_string() ? resp["error"].get<std::string>() : "script error";
    if (!resp.contains("outputs") || !resp["outputs"].is_object()) return "script printed no \"outputs\" object";

    for (size_t k = 0; k < n.outputs.size(); ++k) {
        const auto& name = n.outputs[k].name;
        if (!resp["outputs"].contains(name)) return fmt::format("script did not produce output '{}'", name);
        std::string err;
        if (!ValueFromJson(resp["outputs"][name], n.outputs[k].type, out[k], err))
            return fmt::format("output '{}': {}", name, err);
    }
    return "";
}

bool BodyPython(AppState& s, Node& n) {
    bool changed = false;
    const std::string script = TextParam(n, "script");
    namespace fs = std::filesystem;
    ImGui::Text("%s", script.empty() ? "<no script>" : fs::path(script).filename().string().c_str());
#if !defined(__EMSCRIPTEN__)
    if (ImGui::SmallButton("Browse...")) {
        auto sel = pfd::open_file("Choose a node script", ".",
                                  {"Python scripts", "*.py", "All files", "*"})
                       .result();
        if (!sel.empty()) {
            n.params["script"] = Value::S(sel.front());
            n.error = DescribePythonNode(s, n);
            changed = true;
        }
    }
    ImGui::SameLine();
#endif
    if (ImGui::SmallButton("Reload") && !script.empty()) {
        n.error = DescribePythonNode(s, n);
        changed = true;
    }
    if (!ScriptingAvailable()) ImGui::TextDisabled("(unavailable on web)");
    return changed;
}

}  // namespace

std::string DescribePythonNode(AppState& s, Node& n) {
    const std::string script = TextParam(n, "script");
    if (script.empty()) return "no script set";
    GraphSystem& gs = s.GraphSys();
    RunResult r = RunScript(gs.pythonExe, script, "--describe", "");
    if (!r.ok) return r.error;
    json spec = json::parse(r.output, nullptr, false);
    if (spec.is_discarded() || !spec.is_object()) return "--describe did not print a JSON object";
    std::string err;
    std::vector<PinSpec> ins, outs;
    if (!ParsePins(spec.value("inputs", json()), ins, err)) return err;
    if (!ParsePins(spec.value("outputs", json()), outs, err)) return err;
    n.inputs = std::move(ins);
    n.outputs = std::move(outs);
    n.outValues.assign(n.outputs.size(), Value{});
    if (spec.contains("name") && spec["name"].is_string())
        n.title = fmt::format("{} {}", spec["name"].get<std::string>(), n.id);
    // Drop links into pins that no longer exist -- on whichever graph owns
    // this node (the Node Graph, the Graph Canvas, or a panel graph).
    if (Graph* g = OwningGraph(gs, n)) g->PruneLinks();
    return "";
}

void RegisterPythonNode(NodeTypeRegistry& r) {
    r.Register({"script.python", "Python Script", NodeKind::Other, "Scripting",
                "Run an external script; pins come from `script --describe`.",
                {}, {},   // pins are per-instance, discovered from the script
                &EvalPython, &BodyPython});
}

}  // namespace graph
