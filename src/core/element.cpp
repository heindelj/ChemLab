#include "core/element.h"

#include <map>
#include <string>
#include <vector>

#include "core/atomic_data.h"
#include "core/chemical_data.h"

namespace {

struct ElementEntry {
    Color color{255, 20, 147, 255};   // magenta: "no data"
    float vdw = 1.8f;
    float covalent = 0.0f;
    bool known = false;
};

// Z-indexed table built once from the symbol-keyed maps in atomic_data.h.
const std::vector<ElementEntry>& Table() {
    static const std::vector<ElementEntry> table = [] {
        std::vector<ElementEntry> t(119);
        const float vdwDefault = atomVdwRadii.at("default");
        for (int32_t z = 1; z < (int32_t)t.size(); ++z) {
            const std::string sym = ZToSymbol(z);
            if (sym == "?") continue;
            const auto c = atomColors.find(sym);
            const auto v = atomVdwRadii.find(sym);
            const auto r = covalentRadii.find(sym);
            if (c != atomColors.end()) t[z].color = c->second;
            t[z].vdw = v != atomVdwRadii.end() ? v->second : vdwDefault;
            if (r != covalentRadii.end()) t[z].covalent = r->second;
            t[z].known = c != atomColors.end() && r != covalentRadii.end();
        }
        return t;
    }();
    return table;
}

const ElementEntry& Entry(int32_t z) {
    static const ElementEntry fallback{};
    const auto& t = Table();
    return (z >= 0 && z < (int32_t)t.size()) ? t[(size_t)z] : fallback;
}

}  // namespace

Color ElementColor(int32_t z) { return Entry(z).color; }
float VdwRadius(int32_t z) { return Entry(z).vdw; }
float CovalentRadius(int32_t z) { return Entry(z).covalent; }
bool HasElementData(int32_t z) { return Entry(z).known; }
