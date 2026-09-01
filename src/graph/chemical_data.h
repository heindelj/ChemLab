#pragma once
// graph::ChemicalData -- the standardized chemical-data currency of the node
// system. Required: flat cartesian coordinates R (doubles, xyzxyz...), atomic
// numbers Z (0 = context-dependent wildcard) and natoms. Optional: any number
// of named topologies (pairs of atom indices; distance-based bond perception
// auto-populates one named "bonds"), a 9-number cell matrix (rows = lattice
// vectors) for periodic systems, and arbitrary associated data as a byte
// buffer plus layout specs (name, dtype, starting byte, entry count).
//
// Contained in src/graph on purpose: core Atoms/Frames convert into this at
// source nodes; migrating the core onto it is a separate, later decision.

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace graph {

enum class DType : uint8_t { F32, F64, I32, I64, U8 };

size_t DTypeSize(DType t);
const char* DTypeName(DType t);
bool DTypeFromName(const std::string& name, DType& out);

// Layout spec: a named typed view into ChemicalData::bytes.
struct FieldSpec {
    std::string name;
    DType dtype = DType::F64;
    size_t byteOffset = 0;   // starting byte in `bytes`
    size_t count = 0;        // number of dtype entries
};

// Atom-index pairs; an index i points at Z[i] and the coordinate triple
// R[3i..3i+2].
struct Topology {
    std::string name;                                 // "bonds", "hbonds", ...
    std::vector<std::pair<int32_t, int32_t>> pairs;
};

struct ChemicalData {
    uint32_t natoms = 0;                        // == Z.size()
    std::vector<double> R;                      // 3 * natoms, flat
    std::vector<int32_t> Z;                     // atomic numbers, 0 = wildcard
    std::vector<Topology> topologies;
    std::optional<std::array<double, 9>> cell;  // rows = lattice vectors a, b, c
    std::vector<uint8_t> bytes;                 // arbitrary associated data...
    std::vector<FieldSpec> fields;              // ...described by these layouts

    // "" when consistent, else a description of the problem.
    std::string Validate() const;

    const Topology* FindTopology(const std::string& name) const;
    const FieldSpec* FindField(const std::string& name) const;

    // Append `count` entries of T to `bytes` and record the layout spec.
    template <typename T>
    void AddField(const std::string& name, const T* data, size_t count, DType dtype) {
        FieldSpec f{name, dtype, bytes.size(), count};
        const auto* p = reinterpret_cast<const uint8_t*>(data);
        bytes.insert(bytes.end(), p, p + count * sizeof(T));
        fields.push_back(std::move(f));
    }
    // Typed read access; null when missing, wrong dtype, or out of bounds.
    template <typename T>
    const T* FieldAs(const std::string& name, DType dtype, size_t* count = nullptr) const {
        const FieldSpec* f = FindField(name);
        if (!f || f->dtype != dtype || DTypeSize(dtype) != sizeof(T)) return nullptr;
        if (f->byteOffset + f->count * sizeof(T) > bytes.size()) return nullptr;
        if (count) *count = f->count;
        return reinterpret_cast<const T*>(bytes.data() + f->byteOffset);
    }
};

// Element symbol ("H", "Fe") -> atomic number; 0 on unknown. Tolerant of
// case and trailing digits/underscores ("H1", "Ca_a").
int32_t SymbolToZ(const std::string& label);
const char* ZToSymbol(int32_t z);   // "?" when out of range

// JSON bridge for the script protocol: R flat, Z ints, topologies
// [{name, pairs:[[i,j],...]}], cell 9 numbers or null, bytes base64.
nlohmann::json ChemicalDataToJson(const ChemicalData& c);
bool ChemicalDataFromJson(const nlohmann::json& j, ChemicalData& out, std::string& err);

}  // namespace graph
