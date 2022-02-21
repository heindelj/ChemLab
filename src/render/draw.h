#pragma once

///////////////////////////////////////////////////////////////////////////////
// This file contains most rendering operations we might need to do.	     //
// So, it handles drawing different styles of molecule. Displaying text 	 //
// for geometry information. Drawing hydrogen bonds and lines to the cursor, //
// highlighting atoms, etc. Everything graphics goes in here. 				 //
// Different modes then call these functions as needed. 					 //
///////////////////////////////////////////////////////////////////////////////

void OnWindowResize(ActiveContext& context) {
	if(IsWindowResized()) {
		context.screenWidth = GetScreenWidth();
		context.screenHeight = GetScreenHeight();

		// reset texture width and height for outline shader
		float textureSize[2] = { (float)context.screenWidth, (float)context.screenHeight };
		int textureSizeLoc = GetShaderLocation(context.renderContext.outlineShader, "textureSize");
		SetShaderValue(context.renderContext.outlineShader, textureSizeLoc, textureSize, SHADER_UNIFORM_VEC2);

		// get a new render texture
		context.renderContext.renderTarget  = LoadRenderTexture(context.screenWidth, context.screenHeight);
	}
}

void OnHideUI(ActiveContext& context) {
	context.drawUI = !context.drawUI;

	// TODO: Eventually I'll figure out how to translate and scale the model to fill a portion of the screen.
	// This will be needed to allow split screens and all that.

	//// loop through render contexts and model transforms to fit in their resized windows.
	//Vector2 windowCenter = RectangleCenter(context.windows[0].rect);
	////float fractionOfScreenArea = AspectRatio(context.windows[0].rect) / (context.screenWidth / context.screenHeight);
	//Vector3 cameraForward = normalize(context.renderContext.camera.target - context.renderContext.camera.position);
	//Vector3 cameraRight = cross(cameraForward, context.renderContext.camera.up);
	//float aspectRatio = context.screenWidth / context.screenHeight;
	//Matrix perspective = MatrixPerspective(context.renderContext.camera.fovy, aspectRatio, 0.1, 100.0); // not sure what near and far clipping planes really are.
	//context.renderContext.model->modelTransform = MatrixTranslate(cameraRight);
}

void UpdateLighting(RenderContext& renderContext) {
	SetShaderValue(renderContext.lightingShader, renderContext.lightingShader.locs[SHADER_LOC_VECTOR_VIEW], &renderContext.camera.position.x, SHADER_UNIFORM_VEC3);
	renderContext.light.position = renderContext.camera.position;
	renderContext.light.target   = renderContext.camera.target;
	UpdateLightValues(renderContext.lightingShader, renderContext.light);
}

///////////////////////////////////
////// MODEL DRAWING METHODS //////
///////////////////////////////////

// BALL AND STICK //
void BallAndStickModel::Draw() {
	// If you color the alpha of the material, the output will respect the alpha value
	// ColorAlpha(this->materials[i].maps[MATERIAL_MAP_DIFFUSE].color, 0.3f);
	
	for(int i = 0; i < this->numSpheres; i++) {
		DrawMesh(this->sphereMesh, this->materials[i], this->transforms[i] * this->modelTransform);
	}
	for(int i = this->numSpheres; i < (this->numSpheres + this->numSticks); i++) {
		DrawMesh(this->stickMesh, this->materials[i], this->transforms[i] * this->modelTransform);
	}
}

void BallAndStickModel::DrawHighlighted(const std::set<int>& indices) {
	rlDisableDepthMask();
	Material outlineMaterial = LoadMaterialDefault();
    outlineMaterial.maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
    Matrix scaleTransform = MatrixIdentity();
    Matrix rotationTransform = MatrixIdentity();
    Matrix positionTransform = MatrixIdentity();
	for(auto it = indices.begin(); it != indices.end(); it++) {
		scaleTransform = MatrixScale((Vector3){1.10f, 1.10f, 1.10f}) * MatrixScale(ScaleVectorFromTransform(this->transforms[*it]));
		rotationTransform = RotationTransformFromTransform(this->transforms[*it]);
		positionTransform = MatrixTranslate(PositionVectorFromTransform(this->transforms[*it]));
		DrawMesh(this->sphereMesh, outlineMaterial, scaleTransform * rotationTransform * positionTransform);
	}
	rlEnableDepthMask();
}

// SPHERES //
void SpheresModel::Draw() {
	for(int i = 0; i < this->numSpheres; i++)
		DrawMesh(this->sphereMesh, this->materials[i], this->transforms[i]);
}

void SpheresModel::DrawHighlighted(const std::set<int>& indices) {
	rlDisableDepthMask();
	Material outlineMaterial = LoadMaterialDefault();
    outlineMaterial.maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
    Matrix scaleTransform = MatrixIdentity();
    Matrix rotationTransform = MatrixIdentity();
    Matrix positionTransform = MatrixIdentity();
	for(auto it = indices.begin(); it != indices.end(); it++) {
		scaleTransform = MatrixScale((Vector3){1.10f, 1.10f, 1.10f}) * MatrixScale(ScaleVectorFromTransform(this->transforms[*it]));
		rotationTransform = RotationTransformFromTransform(this->transforms[*it]);
		positionTransform = MatrixTranslate(PositionVectorFromTransform(this->transforms[*it]));
		DrawMesh(this->sphereMesh, outlineMaterial, scaleTransform * rotationTransform * positionTransform);
	}
	rlEnableDepthMask();
}


// STICKS //
void SticksModel::Draw() {
	for(int i = 0; i < this->numSticks; i++)
		DrawMesh(this->stickMesh, this->materials[i], this->transforms[i]);
}

void SticksModel::DrawHighlighted(const std::set<int>& indices) {
	rlDisableDepthMask();
	Material outlineMaterial = LoadMaterialDefault();
    outlineMaterial.maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
    Matrix scaleTransform = MatrixIdentity();
    Matrix rotationTransform = MatrixIdentity();
    Matrix positionTransform = MatrixIdentity();
	for(auto it = indices.begin(); it != indices.end(); it++) {
		scaleTransform = MatrixScale((Vector3){1.10f, 1.10f, 1.10f}) * MatrixScale(ScaleVectorFromTransform(this->transforms[*it]));
		rotationTransform = RotationTransformFromTransform(this->transforms[*it]);
		positionTransform = MatrixTranslate(PositionVectorFromTransform(this->transforms[*it]));
		DrawMesh(this->stickMesh, outlineMaterial, scaleTransform * rotationTransform * positionTransform);
	}
	rlEnableDepthMask();
}

//////////////////////////////////
////// LINE DRAWING METHODS //////
//////////////////////////////////

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

void DrawActiveMode(InteractionMode mode) {
	switch(mode) {
		case VIEW:
			DrawText("MODE: VIEW", 10, 10, 30, RAYWHITE);
			break;
		case BUILD:
			DrawText("MODE: BUILD", 10, 10, 30, RAYWHITE);
			break;
		case ANIMATION:
			DrawText("MODE: ANIMATION", 10, 10, 30, RAYWHITE);
			break;
	}
}

// loop over the rendertargets for each window and draw the textures to the screen.
//void DrawWindowRenderTextures(const ActiveContext& context) {
//	DrawTexturePro(context.renderContext.renderTarget.texture, 
//		context.renderContext.window.rect,
//		(Rectangle){0, 0, (float)context.screenWidth, (float)context.screenHeight},
//	(Vector2){0, 0},
//	//(context.renderContext.window.rect.width * context.renderContext.window.rect.height) / (float)(context.screenWidth * context.screenHeight),
//	0.0,
//	WHITE);
//	//DrawTexture(renderContext.renderTarget.texture, renderContext.window.topLeftX, renderContext.window.topLeftY, WHITE);
//}