#pragma once

enum RenderStyle {
	BALL_AND_STICK,
	STICKS,
	SPHERES
};

//void DrawBonds(const Atoms& atoms, const BondList& bondList) {

//}

void DrawBallAndStick(const Atoms& atoms) {
	for (int i = 0; i < atoms.natoms; i++) {
	    DrawSphere(atoms.xyz[i], atoms.renderData[i].vdwRadius * g_ballScale, atoms.renderData[i].color);
		
		// Loop through the covalent bond list and draw cyclinders where there are bonds
		for (int i = 0; i < atoms.covalentBondList.pairs.size(); i++) {
			if (g_colorBonds) {
				Vector3 atom1Pos = atoms.xyz[atoms.covalentBondList.pairs[i].first];
				Vector3 atom2Pos = atoms.xyz[atoms.covalentBondList.pairs[i].second];
				Vector3 differenceVector = Vector3Subtract(atom2Pos, atom1Pos);
				
				Vector3 middlePos = Vector3Add(atom1Pos, Vector3Scale(differenceVector, 0.5));
				DrawCylinderEx(atom1Pos, middlePos, g_stickRadius, g_stickRadius, 12, atoms.renderData[atoms.covalentBondList.pairs[i].first].color);
				DrawCylinderEx(middlePos, atom2Pos, g_stickRadius, g_stickRadius, 12, atoms.renderData[atoms.covalentBondList.pairs[i].second].color);
			} else {
				DrawCylinderEx(atoms.xyz[atoms.covalentBondList.pairs[i].first], atoms.xyz[atoms.covalentBondList.pairs[i].second], g_stickRadius, g_stickRadius, 12, RAYWHITE);
			}

		}

	}
}

void DrawSticks(const Atoms& atoms) {
	return;
}

void DrawSpheres(const Atoms& atoms) {
	for (int i = 0; i < atoms.natoms; i++) {
	    DrawSphere(atoms.xyz[i], atoms.renderData[i].vdwRadius, atoms.renderData[i].color);
	}
}

void DrawAtoms(const Atoms& atoms, RenderStyle style) {
	switch(style) {
		case BALL_AND_STICK :
			DrawBallAndStick(atoms);
			break;
		case STICKS :
			DrawSticks(atoms);
			break;
		case SPHERES :
			DrawSpheres(atoms);
			break;
		default :
			DrawBallAndStick(atoms);
	}
}