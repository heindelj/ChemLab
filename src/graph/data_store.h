#pragma once
// graph::DataStore -- where node-graph outputs land, keyed "<node>.<pin>".
// The versioning mirrors the dataVersion pattern in core/molecule.h so future
// consumers (plots, exports) can cache against it. Deliberately minimal and
// private to src/graph; reshape at will.

#include <cstdint>
#include <map>
#include <string>

#include "graph/value.h"

namespace graph {

class DataStore {
public:
    void Set(const std::string& key, Value v) {
        slots[key] = std::move(v);
        ++version;
    }
    const Value* Get(const std::string& key) const {
        auto it = slots.find(key);
        return it == slots.end() ? nullptr : &it->second;
    }
    void Clear() {
        if (slots.empty()) return;
        slots.clear();
        ++version;
    }
    uint64_t Version() const { return version; }
    const std::map<std::string, Value>& All() const { return slots; }

private:
    std::map<std::string, Value> slots;
    uint64_t version = 0;
};

}  // namespace graph
