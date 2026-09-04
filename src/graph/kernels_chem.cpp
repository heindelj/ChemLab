// Native chemistry kernels: the pieces of ChemLab's own C++ (element tables,
// the cell-list neighbour search, topology queries) exposed as pure kernels
// so workflows can compose them. See executor.h for the kernel contract.

#include <algorithm>

#include <fmt/format.h>

#include "core/chemical_data.h"
#include "core/element.h"
#include "core/neighbor_list.h"
#include "graph/executor.h"

namespace graph {

namespace {

const Value* In(const KernelArgs& a, size_t k) { return k < a.nin ? a.in[k] : nullptr; }

const ChemicalData* ChemIn(const KernelArgs& a, size_t k, std::string& err) {
    const Value* v = In(a, k);
    if (!v) { err = "input 'chem' not connected"; return nullptr; }
    const ChemicalData* c = v->AsChem();
    if (!c) err = "input 'chem' is not chemical data";
    return c;
}

// Per-atom covalent radius from the element table (angstrom), plus the
// largest one (the neighbour-search cutoff is 2*max + tolerance).
std::string CovalentRadii(KernelArgs& a) {
    std::string err;
    const ChemicalData* c = ChemIn(a, 0, err);
    if (!c) return err;
    std::vector<double> radii(c->natoms);
    double maxR = 0.0;
    for (uint32_t i = 0; i < c->natoms; ++i) {
        radii[i] = i < c->Z.size() ? (double)CovalentRadius(c->Z[i]) : 0.0;
        maxR = std::max(maxR, radii[i]);
    }
    a.out[0].v = std::move(radii);
    if (a.nout > 1) a.out[1] = Value::F(maxR);
    return "";
}

// Van der Waals radii, same shape, for anyone building contact searches.
std::string VdwRadii(KernelArgs& a) {
    std::string err;
    const ChemicalData* c = ChemIn(a, 0, err);
    if (!c) return err;
    std::vector<double> radii(c->natoms);
    double maxR = 0.0;
    for (uint32_t i = 0; i < c->natoms; ++i) {
        radii[i] = i < c->Z.size() ? (double)VdwRadius(c->Z[i]) : 0.0;
        maxR = std::max(maxR, radii[i]);
    }
    a.out[0].v = std::move(radii);
    if (a.nout > 1) a.out[1] = Value::F(maxR);
    return "";
}

// All pairs closer than `cutoff` (pin, else the "cutoff" param): i, j and
// the distances, as arrays.
std::string NeighborListKernel(KernelArgs& a) {
    std::string err;
    const ChemicalData* c = ChemIn(a, 0, err);
    if (!c) return err;
    double cutoff = a.FloatParam("cutoff", 2.0);
    if (const Value* v = In(a, 1))
        if (!v->AsFloat(cutoff)) return "'cutoff' must be a number";
    if (cutoff <= 0.0) return "cutoff must be positive";
    if (c->R.size() < (size_t)c->natoms * 3) return "chemical data has fewer coordinates than atoms";
    NeighborList nl = BuildNeighborList(c->R.data(), c->natoms, cutoff);
    a.out[0].v = std::move(nl.i);
    a.out[1].v = std::move(nl.j);
    a.out[2].v = std::move(nl.d);
    return "";
}

// Does the chemical data carry a topology of this name? `has`/`missing` are
// 0/1 (either drives a Gate directly), `count` its number of pairs.
std::string HasTopology(KernelArgs& a) {
    std::string err;
    const ChemicalData* c = ChemIn(a, 0, err);
    if (!c) return err;
    const std::string name = a.TextParam("name", "bonds");
    const Topology* t = c->FindTopology(name);
    a.out[0] = Value::I(t ? 1 : 0);
    a.out[1] = Value::I(t ? 0 : 1);
    if (a.nout > 2) a.out[2] = Value::I(t ? (int64_t)t->pairs.size() : 0);
    return "";
}

// The pairs of a topology as two index arrays.
std::string TopologyPairs(KernelArgs& a) {
    std::string err;
    const ChemicalData* c = ChemIn(a, 0, err);
    if (!c) return err;
    const std::string name = a.TextParam("name", "bonds");
    const Topology* t = c->FindTopology(name);
    if (!t) return fmt::format("no topology '{}'", name);
    std::vector<int64_t> i, j;
    i.reserve(t->pairs.size());
    j.reserve(t->pairs.size());
    for (const auto& [p, q] : t->pairs) { i.push_back(p); j.push_back(q); }
    a.out[0].v = std::move(i);
    a.out[1].v = std::move(j);
    return "";
}

// A copy of the chemical data with topology `name` replaced by (i, j).
std::string WithTopology(KernelArgs& a) {
    std::string err;
    const ChemicalData* c = ChemIn(a, 0, err);
    if (!c) return err;
    const Value* vi = In(a, 1);
    const Value* vj = In(a, 2);
    if (!vi || !vj) return "inputs 'i' and 'j' must both be connected";
    const std::vector<int64_t>* i = vi->AsIntVec();
    const std::vector<int64_t>* j = vj->AsIntVec();
    if (!i || !j) return "'i' and 'j' must be int arrays";
    if (i->size() != j->size()) return fmt::format("'i' has {} entries, 'j' has {}", i->size(), j->size());
    ChemicalData out = *c;
    Topology& t = out.Topo(a.TextParam("name", "bonds"));
    t.pairs.clear();
    t.pairs.reserve(i->size());
    for (size_t k = 0; k < i->size(); ++k) {
        if ((*i)[k] < 0 || (*j)[k] < 0 || (*i)[k] >= (int64_t)c->natoms || (*j)[k] >= (int64_t)c->natoms)
            return fmt::format("pair {} ({}, {}) out of range", k, (*i)[k], (*j)[k]);
        t.pairs.emplace_back((int32_t)(*i)[k], (int32_t)(*j)[k]);
    }
    a.out[0].v = std::move(out);
    return "";
}

}  // namespace

void RegisterChemKernels(KernelTable& t) {
    t.Register("chem.covalent_radii", &CovalentRadii, "per-atom covalent radii from the element table");
    t.Register("chem.vdw_radii", &VdwRadii, "per-atom van der Waals radii from the element table");
    t.Register("chem.neighbor_list", &NeighborListKernel, "cell-list neighbour search: pairs within a cutoff");
    t.Register("chem.has_topology", &HasTopology, "whether a named topology exists");
    t.Register("chem.topology_pairs", &TopologyPairs, "the pairs of a named topology as arrays");
    t.Register("chem.with_topology", &WithTopology, "chemical data with a topology replaced by (i, j)");
}

}  // namespace graph
