#include "graph/node_registry.h"

#include "graph/scene.h"

namespace graph {

const char* KindName(NodeKind k) {
    switch (k) {
        case NodeKind::Build: return "build";
        case NodeKind::Simulate: return "simulate";
        case NodeKind::Analyze: return "analyze";
        case NodeKind::Visualize: return "visualize";
        default: return "other";
    }
}

bool KindFromName(const std::string& s, NodeKind& out) {
    for (NodeKind k : {NodeKind::Build, NodeKind::Simulate, NodeKind::Analyze, NodeKind::Visualize, NodeKind::Other})
        if (s == KindName(k)) { out = k; return true; }
    return false;
}

KindColor ColorOf(NodeKind k) {
    switch (k) {
        case NodeKind::Build: return {0.95f, 0.55f, 0.15f, 1.0f};      // orange
        case NodeKind::Simulate: return {0.65f, 0.40f, 0.95f, 1.0f};   // purple
        case NodeKind::Analyze: return {0.35f, 0.80f, 0.40f, 1.0f};    // green
        case NodeKind::Visualize: return {0.20f, 0.80f, 0.90f, 1.0f};  // cyan
        default: return {0.60f, 0.60f, 0.65f, 1.0f};                  // grey
    }
}

void NodeTypeRegistry::Register(NodeTypeSpec spec) {
    for (auto& t : types)
        if (t.id == spec.id) { t = std::move(spec); return; }
    types.push_back(std::move(spec));
}

const NodeTypeSpec* NodeTypeRegistry::Find(const std::string& id) const {
    for (const auto& t : types)
        if (t.id == id) return &t;
    return nullptr;
}

NodeTypeRegistry& NodeTypes() {
    static NodeTypeRegistry r;
    static bool initialized = [] {
        RegisterBuiltinNodes(r);
        RegisterPlotNodes(r);
        RegisterViewNodes(r);
        RegisterSceneNodes(r);
        RegisterPythonNode(r);
        return true;
    }();
    (void)initialized;
    return r;
}

}  // namespace graph
