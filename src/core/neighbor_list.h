#pragma once
// Uniform-cutoff neighbour search over cartesian coordinates: every pair
// (i < j) closer than `cutoff`, found with a cell list (cell edge = cutoff,
// 27-cell stencil) so the cost is O(N) for the systems ChemLab shows. This is
// the same algorithm PerceiveBonds (chemical_data.cpp) uses internally, pulled
// out so it can be a node of its own: the workflow that detects covalent bonds
// is Neighbor List -> per-pair cutoff arrays -> vectorised compare -> filter,
// and the neighbour list can equally feed an RDF, a hydrogen-bond search or a
// force field's short-range terms.
//
// The result is arrays, not a structure: pair arrays are what vectorised nodes
// operate on, and they map straight onto numpy for script nodes.
//
// No periodic boundaries yet (the cell, when present, is ignored).

#include <cstdint>
#include <vector>

struct NeighborList {
    std::vector<int64_t> i, j;   // pair endpoints, i < j, sorted by (i, j)
    std::vector<double> d;       // distance of each pair (angstrom)
    size_t Count() const { return i.size(); }
};

// `R` is flat xyzxyz..., `natoms` triples. `cutoff` in angstrom.
NeighborList BuildNeighborList(const double* R, uint32_t natoms, double cutoff);
