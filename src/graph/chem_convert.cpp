#include "graph/chem_convert.h"

#include <fmt/format.h>

namespace graph {

bool AtomsToChemicalData(const Atoms& a, ChemicalData& c, std::string& err) {
    c = ChemicalData{};
    c.natoms = a.natoms;
    c.R.reserve((size_t)a.natoms * 3);
    for (const Vector3& r : a.xyz) {
        c.R.push_back(r.x);
        c.R.push_back(r.y);
        c.R.push_back(r.z);
    }
    c.Z.reserve(a.natoms);
    for (const std::string& label : a.labels) {
        const int32_t z = SymbolToZ(label);
        if (z == 0) {
            err = fmt::format("unknown element label '{}'", label);
            return false;
        }
        c.Z.push_back(z);
    }
    // Distance-based bond perception (done at load / `bonds`) auto-populates
    // the "bonds" topology.
    Topology bonds;
    bonds.name = "bonds";
    bonds.pairs.reserve(a.covalentBondList.pairs.size());
    for (const auto& [i, j] : a.covalentBondList.pairs) bonds.pairs.emplace_back((int32_t)i, (int32_t)j);
    c.topologies.push_back(std::move(bonds));
    // c.cell stays empty: plain xyz files carry no lattice.
    return true;
}

bool ChemicalDataToAtoms(const ChemicalData& c, Atoms& a, std::string& err, float bondTolerance) {
    if (std::string v = c.Validate(); !v.empty()) {
        err = v;
        return false;
    }
    a = Atoms{};
    a.natoms = c.natoms;
    a.xyz.reserve(c.natoms);
    a.labels.reserve(c.natoms);
    a.renderData.reserve(c.natoms);
    for (uint32_t i = 0; i < c.natoms; ++i) {
        const int32_t z = c.Z[i];
        if (z <= 0) {
            err = fmt::format("atom {} has wildcard Z = 0; cannot draw it", i + 1);
            return false;
        }
        const std::string label = ZToSymbol(z);
        if (!IsKnownElement(label)) {
            err = fmt::format("atom {}: no render data for Z = {}", i + 1, z);
            return false;
        }
        a.xyz.push_back(Vector3{(float)c.R[3 * i], (float)c.R[3 * i + 1], (float)c.R[3 * i + 2]});
        a.labels.push_back(label);
        a.renderData.push_back(GetRenderData(label));
    }
    if (const Topology* t = c.FindTopology("bonds")) {
        a.covalentBondList.pairs.reserve(t->pairs.size());
        for (const auto& [i, j] : t->pairs) {
            if (i < 0 || j < 0 || i >= (int32_t)c.natoms || j >= (int32_t)c.natoms) {
                err = fmt::format("bond ({}, {}) out of range", i, j);
                return false;
            }
            a.covalentBondList.pairs.emplace_back((uint32_t)i, (uint32_t)j);
        }
    } else {
        a.covalentBondList = MakeCovalentBondList(a, bondTolerance);
    }
    return true;
}

}  // namespace graph
