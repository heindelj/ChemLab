#pragma once

enum RenderStyle {
	BALL_AND_STICK,
	STICKS,
	SPHERES
};

void DrawBallAndStick(const Atoms& atoms) {
	for (int i = 0; i < atoms.natoms; i++) {
	    DrawSphere(atoms.xyz[i], atoms.renderData[i].vdwRadius * g_ballScale, atoms.renderData[i].color);
		
		// Loop through the covalent bond list and draw cyclinders where there are bonds
		for (int i = 0; i < atoms.covalentBondList.pairs.size(); i++) {
			// SPEED: Currently drawing two cylinders to get different colors. Could instead generate single appropriately colored texture.
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

void DrawDashedLineFromPointToPoint(const Vector2& pointA, const Vector2 pointB) {
	//Vector2 mousePosition = GetMousePosition();
	//Vector2 pointScreenSpace = GetWorldToScreen(point, camera);

	// Make evenly spaced points in screen space
	float dashLength = 5.0f; // length of dash in pixels
	Vector2 differenceVector = Vector2Subtract(pointB, pointA);

	int numDashes = (int)(Vector2Length(differenceVector) / dashLength / 2);
	float numPartialDashes = (Vector2Length(differenceVector) / dashLength / 2) - (float)numDashes;

	Vector2 lineBegin = pointA;
	Vector2 lineEnd = Vector2Add(lineBegin, Vector2Scale(differenceVector, dashLength / Vector2Length(differenceVector)));

	for (int i = 0; i < numDashes; i++) {
		DrawLineEx(lineBegin, lineEnd, 2.0f, YELLOW);
		lineBegin = Vector2Add(lineBegin, Vector2Scale(differenceVector, 2 * dashLength / Vector2Length(differenceVector)));
		lineEnd = Vector2Add(lineEnd, Vector2Scale(differenceVector, 2 * dashLength / Vector2Length(differenceVector)));
	}

	// Draw the extra bit that goes to the end point
	lineEnd = Vector2Add(lineBegin, Vector2Scale(differenceVector, numPartialDashes * dashLength / Vector2Length(differenceVector)));
	DrawLineEx(lineBegin, lineEnd, 2.0f, YELLOW);
}

void DrawDashedLineFromPointToCursor(const Vector2& point) {
	DrawDashedLineFromPointToPoint(point, GetMousePosition());
}

void DrawLineBetweenAtoms(const Atoms& atoms, const int i, const int j, const Camera3D& camera) {
	// SPEED: I'm doing this with world to screen space transformations and multiple draw calls.
	// It may be that for visual integrity, we want to use cylinders or for speed we want to use a custom shader.
	// We'll see.

	// Draws a dashed line from atom i to atom j, accounting for the van der Waal's radius of each atom.
	Vector3 differenceVector    = Vector3Subtract(atoms.xyz[j], atoms.xyz[i]);
	Vector3 startPos = Vector3Add(atoms.xyz[i], Vector3Scale(differenceVector, (atoms.renderData[i].vdwRadius * g_ballScale) / Vector3Length(differenceVector)));
	Vector3 endPos   = Vector3Subtract(atoms.xyz[j], Vector3Scale(differenceVector, (atoms.renderData[j].vdwRadius * g_ballScale) / Vector3Length(differenceVector)));

	if (g_dashedLine) {
		DrawDashedLineFromPointToPoint(GetWorldToScreen(startPos, camera), GetWorldToScreen(endPos, camera));
	} else {
		Vector2 startPosScreenSpace = GetWorldToScreen(startPos, camera);
		Vector2 endPosScreenSpace   = GetWorldToScreen(endPos, camera);
		DrawLineEx(startPosScreenSpace, endPosScreenSpace, g_hbondWidth, g_hbondColor);	
	}

}

void OverlaySpheres(const Atoms& atoms) {
	for (int i = 0; i < atoms.natoms; i++) {
		Color color = Fade(atoms.renderData[i].color, g_alphaOverlay);
	    DrawSphere(atoms.xyz[i], atoms.renderData[i].vdwRadius, color);
	}
}

void OverlayNumbers(const Atoms& atoms, const Camera3D& camera) {
	// NOTE(JOE): this must be drawn in 2D mode, not 3D mode

	// Currently this just draw the text independent of zoom.
	// It also seems to be located at the top left, so there should be an offset
	// based on the font size and the camera zoom should be taken into account somehow.
	for (int i = 0; i < atoms.natoms; i++) {
		Vector2 pos = GetWorldToScreen(atoms.xyz[i], camera);
		DrawText(std::to_string(i+1).c_str(), (int)pos.x, (int)pos.y, 12, BLACK);
	}
}