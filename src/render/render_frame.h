#pragma once

///////////////////////////////////////////////////////////////////////////////
// This file contains all rendering operations we might need to do.			 //
// So, it handles drawing different styles of molecule. Displaying text 	 //
// for geometry information. Drawing hydrogen bonds and lines to the cursor, //
// highlighting atoms, etc. Everything graphics goes in here. 				 //
// Different modes then call these functions as needed. 					 //
///////////////////////////////////////////////////////////////////////////////

void UpdateLighting(RenderContext& renderContext) {
	renderContext.light.position = renderContext.camera.position;
	renderContext.light.target   = renderContext.camera.target;
	UpdateLightValues(renderContext.lightingShader, renderContext.light);
}

////// MODEL DRAWING METHODS //////

void BallAndStickModel::Draw() {
	for(int i = 0; i < this->numSpheres; i++)
		DrawMesh(this->sphereMesh, this->materials[i], this->transforms[i]);
	for(int i = this->numSpheres; i < (this->numSpheres + this->numSticks); i++)
		DrawMesh(this->stickMesh, this->materials[i], this->transforms[i]);
}


void SpheresModel::Draw() {
	for(int i = 0; i < this->numSpheres; i++)
		DrawMesh(this->sphereMesh, this->materials[i], this->transforms[i]);
}

void SticksModel::Draw() {
	for(int i = 0; i < this->numSticks; i++)
		DrawMesh(this->stickMesh, this->materials[i], this->transforms[i]);
}

////// LINE DRAWING METHODS //////

void DrawDashedLineFromPointToPoint(const Vector2& pointA, const Vector2& pointB, const float width, const Color& color) {
	// Make evenly spaced points in screen space
	float dashLength = 5.0f; // length of dash in pixels
	Vector2 differenceVector = pointB - pointA;

	int numDashes = (int)(norm(differenceVector) / dashLength / 2);
	float numPartialDashes = (norm(differenceVector) / dashLength / 2) - (float)numDashes;

	Vector2 lineBegin = pointA;
	Vector2 lineEnd = lineBegin + differenceVector * (dashLength / norm(differenceVector));

	for (int i = 0; i < numDashes; i++) {
		DrawLineEx(lineBegin, lineEnd, 2.0f, color);
		lineBegin = lineBegin + differenceVector * (2 * dashLength / norm(differenceVector));
		lineEnd = lineEnd + differenceVector * (2 * dashLength / norm(differenceVector));
	}

	// Draw the extra bit that goes to the end point
	lineEnd = lineBegin + differenceVector * (numPartialDashes * dashLength / norm(differenceVector));
	DrawLineEx(lineBegin, lineEnd, width, color);
}

void DrawDashedLineFromPointToCursor(const Vector2& point, const float width, const Color& color) {
	DrawDashedLineFromPointToPoint(point, GetMousePosition(), width, color);
}

void DrawLineBetweenPoints(const Vector3& v1, const Vector3& v2, const Camera3D& camera, const float width, const Color& color, const bool dashed=true) {
	if (dashed) {
		DrawDashedLineFromPointToPoint(GetWorldToScreen(v1, camera), GetWorldToScreen(v2, camera), width, color);
	} else {
		DrawLineEx(GetWorldToScreen(v1, camera), GetWorldToScreen(v2, camera), width, color);
	}
}

void DrawLineBetweenPoints(const MolecularModel& model, const int i, const int j, const Camera3D& camera, const float width, const Color& color, const bool dashed=true) {

	DrawLineBetweenPoints(PositionVectorFromTransform(model.transforms[i]), PositionVectorFromTransform(model.transforms[j]), camera, width, color, dashed);
}

void DrawLineBetweenAtoms(const Atoms& atoms, const int i, const int j, const Camera3D& camera, const float width, const Color& color, const bool dashed=true) {
	// Draws a dashed line from atom i to atom j, accounting for the van der Waal's radius of each atom.
	Vector3 differenceVector    = atoms.xyz[j] - atoms.xyz[i];
	Vector3 startPos = atoms.xyz[i] + differenceVector * ((atoms.renderData[i].vdwRadius * g_ballScale) / norm(differenceVector));
	Vector3 endPos   = atoms.xyz[j] - differenceVector * ((atoms.renderData[j].vdwRadius * g_ballScale) / norm(differenceVector));

	if (dashed) {
		DrawDashedLineFromPointToPoint(GetWorldToScreen(startPos, camera), GetWorldToScreen(endPos, camera), width, color);
	} else {
		DrawLineEx(GetWorldToScreen(startPos, camera), GetWorldToScreen(endPos, camera), width, color);
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