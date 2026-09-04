#include "core/chemical_data.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

#include "core/element.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

size_t DTypeSize(DType t) {
    switch (t) {
        case DType::F32: return 4;
        case DType::F64: return 8;
        case DType::I32: return 4;
        case DType::I64: return 8;
        case DType::U8: return 1;
    }
    return 0;
}

const char* DTypeName(DType t) {
    switch (t) {
        case DType::F32: return "f32";
        case DType::F64: return "f64";
        case DType::I32: return "i32";
        case DType::I64: return "i64";
        case DType::U8: return "u8";
    }
    return "?";
}

bool DTypeFromName(const std::string& name, DType& out) {
    if (name == "f32") out = DType::F32;
    else if (name == "f64") out = DType::F64;
    else if (name == "i32") out = DType::I32;
    else if (name == "i64") out = DType::I64;
    else if (name == "u8") out = DType::U8;
    else return false;
    return true;
}

std::string ChemicalData::Validate() const {
    if (Z.size() != natoms) return fmt::format("natoms is {} but Z has {} entries", natoms, Z.size());
    if (R.size() != (size_t)natoms * 3)
        return fmt::format("R has {} numbers, expected 3*natoms = {}", R.size(), natoms * 3);
    for (const Topology& t : topologies)
        for (const auto& [a, b] : t.pairs)
            if (a < 0 || b < 0 || a >= (int32_t)natoms || b >= (int32_t)natoms)
                return fmt::format("topology '{}' has pair ({}, {}) out of range 0..{}", t.name, a, b,
                                   (int)natoms - 1);
    if (!labels.empty() && labels.size() != natoms)
        return fmt::format("labels has {} entries, expected natoms = {}", labels.size(), natoms);
    for (const FieldSpec& f : fields)
        if (f.byteOffset + f.count * DTypeSize(f.dtype) > bytes.size())
            return fmt::format("field '{}' extends past the byte buffer", f.name);
    return "";
}

const Topology* ChemicalData::FindTopology(const std::string& name) const {
    for (const auto& t : topologies)
        if (t.name == name) return &t;
    return nullptr;
}

std::string ChemicalData::Label(size_t i) const {
    if (i < labels.size()) return labels[i];
    return i < Z.size() ? ZToSymbol(Z[i]) : "?";
}

Topology* ChemicalData::FindTopology(const std::string& name) {
    for (auto& t : topologies)
        if (t.name == name) return &t;
    return nullptr;
}

Topology& ChemicalData::Topo(const std::string& name) {
    if (Topology* t = FindTopology(name)) return *t;
    topologies.push_back(Topology{name, {}});
    return topologies.back();
}

size_t ChemicalData::BondCount() const {
    const Topology* t = FindTopology("bonds");
    return t ? t->pairs.size() : 0;
}

const FieldSpec* ChemicalData::FindField(const std::string& name) const {
    for (const auto& f : fields)
        if (f.name == name) return &f;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Element symbols
// ---------------------------------------------------------------------------

namespace {
constexpr const char* kSymbols[] = {
    "?",  "H",  "He", "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne", "Na", "Mg", "Al", "Si", "P",  "S",
    "Cl", "Ar", "K",  "Ca", "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn", "Ga", "Ge", "As",
    "Se", "Br", "Kr", "Rb", "Sr", "Y",  "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd", "In", "Sn",
    "Sb", "Te", "I",  "Xe", "Cs", "Ba", "La", "Ce", "Pr", "Nd", "Pm", "Sm", "Eu", "Gd", "Tb", "Dy", "Ho",
    "Er", "Tm", "Yb", "Lu", "Hf", "Ta", "W",  "Re", "Os", "Ir", "Pt", "Au", "Hg", "Tl", "Pb", "Bi", "Po",
    "At", "Rn", "Fr", "Ra", "Ac", "Th", "Pa", "U",  "Np", "Pu", "Am", "Cm", "Bk", "Cf", "Es", "Fm", "Md",
    "No", "Lr", "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds", "Rg", "Cn", "Nh", "Fl", "Mc", "Lv", "Ts", "Og"};
constexpr int32_t kMaxZ = (int32_t)(sizeof(kSymbols) / sizeof(kSymbols[0])) - 1;
}  // namespace

int32_t SymbolToZ(const std::string& label) {
    // Strip everything after the leading alphabetic part ("H1", "Ca_a").
    std::string sym;
    for (char c : label) {
        if (!isalpha((unsigned char)c)) break;
        sym += c;
        if (sym.size() == 2) break;
    }
    if (sym.empty()) return 0;
    sym[0] = (char)toupper((unsigned char)sym[0]);
    if (sym.size() == 2) sym[1] = (char)tolower((unsigned char)sym[1]);
    for (int32_t z = 1; z <= kMaxZ; ++z)
        if (sym == kSymbols[z]) return z;
    // Two-letter guess failed ("HW" from water models): retry the first letter.
    if (sym.size() == 2) {
        sym.resize(1);
        for (int32_t z = 1; z <= kMaxZ; ++z)
            if (sym == kSymbols[z]) return z;
    }
    return 0;
}

const char* ZToSymbol(int32_t z) { return (z >= 1 && z <= kMaxZ) ? kSymbols[z] : "?"; }

// ---------------------------------------------------------------------------
// JSON bridge (base64 for the byte buffer)
// ---------------------------------------------------------------------------

namespace {

constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string B64Encode(const std::vector<uint8_t>& in) {
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        uint32_t n = (uint32_t)in[i] << 16;
        if (i + 1 < in.size()) n |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < in.size()) n |= in[i + 2];
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += i + 1 < in.size() ? kB64[(n >> 6) & 63] : '=';
        out += i + 2 < in.size() ? kB64[n & 63] : '=';
    }
    return out;
}

bool B64Decode(const std::string& in, std::vector<uint8_t>& out) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    out.clear();
    uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        const int v = val(c);
        if (v < 0) return false;
        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)(buf >> bits));
        }
    }
    return true;
}

}  // namespace

json ChemicalDataToJson(const ChemicalData& c) {
    json j;
    j["natoms"] = c.natoms;
    j["R"] = c.R;
    j["Z"] = c.Z;
    j["topologies"] = json::array();
    for (const Topology& t : c.topologies) {
        json pairs = json::array();
        for (const auto& [a, b] : t.pairs) pairs.push_back({a, b});
        j["topologies"].push_back({{"name", t.name}, {"pairs", std::move(pairs)}});
    }
    if (c.cell) j["cell"] = *c.cell;
    else j["cell"] = nullptr;
    if (!c.labels.empty()) j["labels"] = c.labels;
    j["fields"] = json::array();
    for (const FieldSpec& f : c.fields)
        j["fields"].push_back(
            {{"name", f.name}, {"dtype", DTypeName(f.dtype)}, {"offset", f.byteOffset}, {"count", f.count}});
    j["bytes"] = B64Encode(c.bytes);
    return j;
}

bool ChemicalDataFromJson(const json& j, ChemicalData& out, std::string& err) {
    out = ChemicalData{};
    if (!j.is_object()) { err = "chemdata must be a JSON object"; return false; }
    try {
        if (!j.contains("Z") || !j["Z"].is_array()) { err = "chemdata needs an integer array \"Z\""; return false; }
        out.Z = j["Z"].get<std::vector<int32_t>>();
        out.natoms = j.contains("natoms") ? j["natoms"].get<uint32_t>() : (uint32_t)out.Z.size();
        if (!j.contains("R") || !j["R"].is_array()) { err = "chemdata needs a flat number array \"R\""; return false; }
        out.R = j["R"].get<std::vector<double>>();
        for (const auto& t : j.value("topologies", json::array())) {
            Topology topo;
            topo.name = t.value("name", "");
            for (const auto& p : t.value("pairs", json::array())) {
                if (!p.is_array() || p.size() != 2) { err = "topology pairs must be [i, j]"; return false; }
                topo.pairs.emplace_back(p[0].get<int32_t>(), p[1].get<int32_t>());
            }
            out.topologies.push_back(std::move(topo));
        }
        if (j.contains("cell") && !j["cell"].is_null()) {
            const auto cell = j["cell"].get<std::vector<double>>();
            if (cell.size() != 9) { err = "cell must hold 9 numbers (rows = lattice vectors)"; return false; }
            std::array<double, 9> a{};
            std::copy(cell.begin(), cell.end(), a.begin());
            out.cell = a;
        }
        if (j.contains("labels") && j["labels"].is_array()) out.labels = j["labels"].get<std::vector<std::string>>();
        if (j.contains("bytes") && j["bytes"].is_string() && !B64Decode(j["bytes"].get<std::string>(), out.bytes)) {
            err = "bytes is not valid base64";
            return false;
        }
        for (const auto& f : j.value("fields", json::array())) {
            FieldSpec spec;
            spec.name = f.value("name", "");
            if (!DTypeFromName(f.value("dtype", ""), spec.dtype)) { err = fmt::format("field '{}': unknown dtype", spec.name); return false; }
            spec.byteOffset = f.value("offset", (size_t)0);
            spec.count = f.value("count", (size_t)0);
            out.fields.push_back(std::move(spec));
        }
    } catch (const json::exception& e) {
        err = fmt::format("malformed chemdata: {}", e.what());
        return false;
    }
    err = out.Validate();
    return err.empty();
}

// ---------------------------------------------------------------------------
// Bond perception
// ---------------------------------------------------------------------------

void PerceiveBonds(ChemicalData& c, float tolerance) {
    // Eq. (1) of "A rule-based algorithm for automatic bond type perception",
    // J. Cheminformatics 4, 26 (2012). Only the distance criterion for now.
    // Implemented with a uniform cell grid (cell size = max cutoff) so the
    // search is O(N) instead of O(N^2); large systems load interactively.
    Topology& bonds = c.Topo("bonds");
    bonds.pairs.clear();
    const uint32_t n = c.natoms;
    if (n < 2 || c.R.size() < (size_t)n * 3) return;

    std::vector<float> rcov(n);
    float maxCovalent = 0.0f;
    double lo[3], hi[3];
    for (int k = 0; k < 3; ++k) lo[k] = hi[k] = c.R[k];
    for (uint32_t i = 0; i < n; i++) {
        rcov[i] = i < c.Z.size() ? CovalentRadius(c.Z[i]) : 0.0f;
        maxCovalent = std::max(maxCovalent, rcov[i]);
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], c.R[3 * i + k]);
            hi[k] = std::max(hi[k], c.R[3 * i + k]);
        }
    }
    const double cell = std::max(2.0 * maxCovalent + tolerance, 1e-3);

    const auto cellIndex = [&](uint32_t i, int& cx, int& cy, int& cz) {
        cx = (int)((c.R[3 * i] - lo[0]) / cell);
        cy = (int)((c.R[3 * i + 1] - lo[1]) / cell);
        cz = (int)((c.R[3 * i + 2] - lo[2]) / cell);
    };
    // Cells run 0..n-1; the neighbour search touches -1..n, so the key uses
    // n+2 per axis (a smaller stride would alias neighbouring cells and
    // count pairs twice).
    const int64_t ny = (int64_t)((hi[1] - lo[1]) / cell) + 3;
    const int64_t nz = (int64_t)((hi[2] - lo[2]) / cell) + 3;

    // Bucket atoms by cell (hash map keeps memory bounded for sparse systems).
    std::unordered_map<int64_t, std::vector<uint32_t>> grid;
    grid.reserve(n);
    const auto key = [&](int cx, int cy, int cz) -> int64_t {
        return (((int64_t)cx + 1) * ny + (cy + 1)) * nz + (cz + 1);
    };
    for (uint32_t i = 0; i < n; i++) {
        int cx, cy, cz;
        cellIndex(i, cx, cy, cz);
        grid[key(cx, cy, cz)].push_back(i);
    }

    for (uint32_t i = 0; i < n; i++) {
        int cx, cy, cz;
        cellIndex(i, cx, cy, cz);
        for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
            const auto it = grid.find(key(cx + dx, cy + dy, cz + dz));
            if (it == grid.end()) continue;
            for (uint32_t j : it->second) {
                if (j <= i) continue;   // each pair once
                const double cutoff = rcov[i] + rcov[j] + tolerance;
                const double ddx = c.R[3 * i] - c.R[3 * j], ddy = c.R[3 * i + 1] - c.R[3 * j + 1],
                             ddz = c.R[3 * i + 2] - c.R[3 * j + 2];
                if (ddx * ddx + ddy * ddy + ddz * ddz < cutoff * cutoff)
                    bonds.pairs.emplace_back((int32_t)i, (int32_t)j);
            }
        }
    }
    std::sort(bonds.pairs.begin(), bonds.pairs.end());
}
