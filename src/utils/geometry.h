#pragma once

BondList MakeCovalentBondList(const Atoms& atoms) {
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

inline float distance(const Vector3& r1, const Vector3& r2) {
	return norm(r1 - r2);
}

// angle between two vectors
inline float angle(const Vector3& r1, const Vector3& r2) {
	return acosf(dot(r1, r2) / (norm(r1) * norm(r2)));
}

inline float angleDeg(const Vector3& r1, const Vector3& r2) {
	return angle(r1, r2) * RAD2DEG;
}

// angle between three points in space
inline float angle(const Vector3& r1, const Vector3& r2, const Vector3& r3) {
	return angle(r2 - r1, r3 - r2);
}

inline float angleDeg(const Vector3& r1, const Vector3& r2, const Vector3& r3) {
	return angleDeg(r2 - r1, r3 - r2);
}

// dihedral angle between three vectors
inline float dihedral(const Vector3& r1, const Vector3& r2, const Vector3& r3) {
	Vector3 n1 = cross(r1, r2) / norm(cross(r1, r2));
	Vector3 n2 = cross(r2, r3) / norm(cross(r2, r3));
	Vector3 m1 = cross(n1, r2 / norm(r2));
	return atan2(dot(m1, n2), dot(n1, n2));
}

inline float dihedralDeg(const Vector3& r1, const Vector3& r2, const Vector3& r3) {
	return dihedral(r1, r2, r3) * RAD2DEG;
}

// dihedral angle between four points in space
inline float dihedral(const Vector3& r1, const Vector3& r2, const Vector3& r3, const Vector3& r4) {

	return dihedral(r2 - r1, r3 - r2, r4 - r3);
}

inline float dihedralDeg(const Vector3& r1, const Vector3& r2, const Vector3& r3, const Vector3& r4) {
	return dihedral(r2 - r1, r3 - r2, r4 - r3) * RAD2DEG;
}