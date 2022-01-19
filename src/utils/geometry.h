#pragma once

BondList makeCovalentBondList(const Atoms& atoms) {
	// Determines if two atoms are covalently bonded based on
	// their covalent radii. Currently implement just Eq. (1) from
	// the below paper.
	//
	// TODO: Implement the entire algorithm contained in:
	// A rule-based algorithm for automatic bond type perception
	// Journal of Cheminformatics volume 4, Article number: 26 (2012)
	std::vector<std::pair<uint32_t, uint32_t>> pairs;
	for (int i = 0; i < atoms.natoms - 1; i++) {
		for (int j = i+1; j < atoms.natoms; j++) {
			if (Vector3Length(Vector3Subtract(atoms.xyz[i], atoms.xyz[j])) < (atoms.renderData[i].covalentRadius + atoms.renderData[j].covalentRadius + 0.4f))
				pairs.push_back(std::make_pair(i, j));
		}
	}
	return (BondList){pairs};
}