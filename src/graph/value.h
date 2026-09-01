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

#include "graph/chemical_data.h"

namespace graph {

enum class ValueType { Any, Float, Int, Text, FloatVec, Positions, Labels, IntVec, Chem };

// N x 3 cartesian coordinates, flat xyzxyz... (angstrom).
struct Positions {
    std::vector<double> xyz;
    size_t Count() const { return xyz.size() / 3; }
};

using Labels = std::vector<std::string>;

struct Value {
    std::variant<std::monostate, double, int64_t, std::string, std::vector<double>, Positions, Labels,
                 std::vector<int64_t>, ChemicalData>
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
