#include "graph/node_registry.h"

namespace graph {

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
        RegisterPythonNode(r);
        return true;
    }();
    (void)initialized;
    return r;
}

}  // namespace graph
