#pragma once
// graph::Value -- the typed values that flow along node graph links, plus the
// JSON bridge used by the external-script protocol.
//
// Everything about how node-generated data is represented lives in src/graph
// on purpose: the rest of the app only sees GraphSystem (app_state.h holds it
// behind a forward declaration), so this representation can change freely.

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "core/chemical_data.h"
#include "plot/plot_spec.h"

namespace graph {

enum class ValueType { Any, Float, Int, Text, FloatVec, Positions, Labels, IntVec, Chem, Table, Series, Structure, Panel };

// N x 3 cartesian coordinates, flat xyzxyz... (angstrom).
struct Positions {
    std::vector<double> xyz;
    size_t Count() const { return xyz.size() / 3; }
};

using Labels = std::vector<std::string>;

// Numeric tabular data (a CSV file, say): named columns of equal length.
// Column-major so a column can be handed out as a FloatVec without copying.
struct Table {
    std::vector<std::string> columns;            // column names
    std::vector<std::vector<double>> data;       // data[c][row]
    size_t Rows() const { return data.empty() ? 0 : data[0].size(); }
    size_t Cols() const { return columns.size(); }
    int FindColumn(const std::string& name) const;   // -1 when absent
};

// One plot series (see plot/plot_spec.h); flows from Series nodes into Plot nodes.
using Series = plot::Series;

// A handle to one of the loaded structures (AppState::structures): a whole
// trajectory, not a frame. Deliberately light -- the frames stay owned by the
// app; nodes that need coordinates go through a Select Frame node, which
// yields a ChemicalData. `index` is resolved at evaluation time and may be
// stale after structures are removed; `path`/`name` identify it durably.
struct StructureHandle {
    std::string name;
    std::string path;
    int index = -1;
    int frames = 0;
};

// Panels placed in a scene layout (scene.h): a Panel node yields one, a Tabs
// node several (they become tabs, in order), a Layout node's slot pins take
// them. `visible` = shown at startup (the classic layout docks the Console
// but keeps it closed).
struct PanelRef {
    std::string panel;   // panel id from the panel registry ("structure_view")
    bool visible = true;
};
using PanelList = std::vector<PanelRef>;

struct Value {
    std::variant<std::monostate, double, int64_t, std::string, std::vector<double>, Positions, Labels,
                 std::vector<int64_t>, ChemicalData, Table, Series, StructureHandle, PanelList>
        v;

    Value() = default;
    static Value F(double x) { Value r; r.v = x; return r; }
    static Value I(int64_t x) { Value r; r.v = x; return r; }
    static Value S(std::string x) { Value r; r.v = std::move(x); return r; }

    bool Empty() const { return std::holds_alternative<std::monostate>(v); }
    ValueType Type() const;

    // Loose numeric accessors (Int and Float coerce into each other).
    bool AsFloat(double& out) const;
    bool AsInt(int64_t& out) const;
    const std::string* AsText() const;
    const std::vector<double>* AsFloatVec() const;
    const Positions* AsPositions() const;
    const Labels* AsLabels() const;
    const std::vector<int64_t>* AsIntVec() const;
    const ChemicalData* AsChem() const;
    const Table* AsTable() const;
    const Series* AsSeries() const;
    const StructureHandle* AsStructure() const;
    const PanelList* AsPanels() const;

    // Short human-readable form for node bodies / the console.
    std::string Preview(size_t maxItems = 4) const;
};

const char* TypeName(ValueType t);
bool TypeFromName(const std::string& name, ValueType& out);
// May a value of type `from` be plugged into an input of type `to`?
bool Compatible(ValueType from, ValueType to);

// JSON bridge for the script protocol (see py_runner.h for the protocol).
nlohmann::json ValueToJson(const Value& v);
// Decode `j` as a value of `expected` type (Any = infer). False + err on shape mismatch.
bool ValueFromJson(const nlohmann::json& j, ValueType expected, Value& out, std::string& err);

}  // namespace graph
