#pragma once

enum RenderStyle {
	BALL_AND_STICK,
	STICKS,
	SPHERES
};

//void DrawBonds(const Atoms& atoms, const BondList& bondList) {

//}

void DrawBallAndStick(const Atoms& atoms){//, const BondList& bondList) {
	for (int i = 0; i < atoms.natoms; i++) {
	    DrawSphere(atoms.xyz[i], atoms.renderData[i].vdwRadius * g_ballScale, atoms.renderData[i].color);
		DrawCylinderEx(atoms.xyz[0], atoms.xyz[1], g_stickRadius, g_stickRadius, 12, RAYWHITE);
		// Loop through the covalent bond list and draw cyclinders where there are bonds
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