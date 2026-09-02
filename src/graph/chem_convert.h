#pragma once
// Conversions between the core Atoms (what files load into and the renderer
// draws) and graph::ChemicalData (what flows along links). Source nodes go
// Atoms -> ChemicalData; the Render 3D node goes back the other way so that
// anything producing ChemicalData -- a script, an analysis -- can be drawn.

#include <string>

#include "core/molecule.h"
#include "graph/chemical_data.h"

namespace graph {

// R, Z and the "bonds" topology from the atoms' covalent bond list.
// False + err on an unknown element label.
bool AtomsToChemicalData(const Atoms& atoms, ChemicalData& out, std::string& err);

// Labels from Z, render data from the element tables, bonds from the "bonds"
// topology when present, else from distance-based perception with
// `bondTolerance`. False + err on a wildcard/unknown Z or inconsistent data.
bool ChemicalDataToAtoms(const ChemicalData& chem, Atoms& out, std::string& err, float bondTolerance = 0.4f);

}  // namespace graph
