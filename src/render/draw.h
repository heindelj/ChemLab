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
	//debug(renderContext.camera.position);
	UpdateLightValues(renderContext.lightingShader, renderContext.light);

	// HERE: For some reason the stick lighting direction is not being updated.
}

///////////////////////////////////
////// MODEL DRAWING METHODS //////
///////////////////////////////////

// BALL AND STICK //
void BallAndStickModel::Draw() {
	// If you color the alpha of the material, the output will respect the alpha value
	// ColorAlpha(this->materials[i].maps[MATERIAL_MAP_DIFFUSE].color, 0.3f);
	//double start = GetTime();
	for (int i = 0; i < meshes.size(); ++i) {
		std::vector<RenderingIndices> allIndices = this->meshIndexToRenderingIndices[i];
		for (auto indices : allIndices) { // loops over number of materials to be drawn on this mesh
			//DrawMeshInstanced(this->meshes[i], this->materials[indices.materialIndex],
			//	&this->transforms[indices.materialIndex][indices.firstTransformIndex],
			//	indices.numTransforms);
			for (int j = 0; j < indices.numTransforms; ++j) {
				DrawMesh(this->meshes[i], this->materials[indices.materialIndex],
					this->transforms[indices.materialIndex][indices.firstTransformIndex+j]);
			}
		}
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
		EntityIndices indices = this->IDToEntityIndices[*it];

		scaleTransform = MatrixScale((Vector3){1.10f, 1.10f, 1.10f}) * MatrixScale(ScaleVectorFromTransform(

			this->transforms[indices.materialIndex][indices.transformIndex])
		);
		rotationTransform = RotationTransformFromTransform(this->transforms[indices.materialIndex][indices.transformIndex]);
		positionTransform = MatrixTranslate(PositionVectorFromTransform(this->transforms[indices.materialIndex][indices.transformIndex]));
		DrawMesh(this->sphereMesh, outlineMaterial, scaleTransform * rotationTransform * positionTransform);
	}
	rlEnableDepthMask();
}

// SPHERES //
void SpheresModel::Draw() {
	for(int i = 0; i < this->numSpheres; i++) {
		EntityIndices indices = this->IDToEntityIndices[i];
		DrawMesh(this->sphereMesh, this->materials[i], this->transforms[indices.materialIndex][indices.transformIndex]);
	}
}

void SpheresModel::DrawHighlighted(const std::set<int>& indices) {
	rlDisableDepthMask();
	Material outlineMaterial = LoadMaterialDefault();
    outlineMaterial.maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
    Matrix scaleTransform = MatrixIdentity();
    Matrix rotationTransform = MatrixIdentity();
    Matrix positionTransform = MatrixIdentity();
	for(auto it = indices.begin(); it != indices.end(); it++) {
		EntityIndices indices = this->IDToEntityIndices[*it];
		scaleTransform = MatrixScale((Vector3){1.10f, 1.10f, 1.10f}) * MatrixScale(ScaleVectorFromTransform(
			this->transforms[indices.materialIndex][indices.transformIndex])
		);
		rotationTransform = RotationTransformFromTransform(this->transforms[indices.materialIndex][indices.transformIndex]);
		positionTransform = MatrixTranslate(PositionVectorFromTransform(this->transforms[indices.materialIndex][indices.transformIndex]));
		DrawMesh(this->sphereMesh, outlineMaterial, scaleTransform * rotationTransform * positionTransform);
	}
	rlEnableDepthMask();
}


// STICKS //
void SticksModel::Draw() {
	for(int i = 0; i < this->numSticks; i++) {
		EntityIndices indices = this->IDToEntityIndices[i];
		DrawMesh(this->stickMesh, this->materials[i], this->transforms[indices.materialIndex][indices.transformIndex]);
	}
}

void SticksModel::DrawHighlighted(const std::set<int>& indices) {
	rlDisableDepthMask();
	Material outlineMaterial = LoadMaterialDefault();
    outlineMaterial.maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
    Matrix scaleTransform = MatrixIdentity();
    Matrix rotationTransform = MatrixIdentity();
    Matrix positionTransform = MatrixIdentity();
	for(auto it = indices.begin(); it != indices.end(); it++) {
		EntityIndices indices = this->IDToEntityIndices[*it];
		scaleTransform = MatrixScale((Vector3){1.10f, 1.10f, 1.10f}) * MatrixScale(
			ScaleVectorFromTransform(this->transforms[indices.materialIndex][indices.transformIndex])
			);
		rotationTransform = RotationTransformFromTransform(this->transforms[indices.materialIndex][indices.transformIndex]);
		positionTransform = MatrixTranslate(PositionVectorFromTransform(this->transforms[indices.materialIndex][indices.transformIndex]));
		DrawMesh(this->stickMesh, outlineMaterial, scaleTransform * rotationTransform * positionTransform);
	}
	rlEnableDepthMask();
}

// PLANE MESH //
void DrawPlane(Model& gridModel, const Vector3& normal, const Vector3& point) {
	rlDisableBackfaceCulling();
	// a plane is defined by a normal vector and a point on the plane.
	gridModel.transform = MatrixTranslate(point) * MatrixAlignToAxis((Vector3){0,1,0}, normalize(normal));

	DrawModelWires(gridModel, Vector3Zero(), 1.0f, WHITE);
	rlEnableBackfaceCulling();
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

void DrawLineBetweenPoints(MolecularModel& model, const int i, const int j, const Camera3D& camera, const float width, const Color& color, const bool dashed=true) {
	EntityIndices indices_i = model.IDToEntityIndices[i];
	EntityIndices indices_j = model.IDToEntityIndices[j];

	DrawLineBetweenPoints(
		PositionVectorFromTransform(model.transforms[indices_i.materialIndex][indices_i.transformIndex]),
		PositionVectorFromTransform(model.transforms[indices_j.materialIndex][indices_j.transformIndex]),
		camera, width, color, dashed);
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

Color ContrastingColor(const Color& color) {
	float r = color.r / 255.0;
	float g = color.g / 255.0;
	float b = color.b / 255.0;
	float alpha = 0.5 * (2 * r - g - b);
	float beta = sqrt(3.0) / 2.0 * (g - b);
	float H2 = atan2(alpha, beta);
	if (H2 >= 60 * DEG2RAD && H2 < 180 * DEG2RAD) {
		return GREEN;
	} else if (H2 >=180 * DEG2RAD && H2 < 300 * DEG2RAD) {
		return BLUE;
	}
	return RED;
}

void OverlayNumbers(const Atoms& atoms, const MolecularModel& model, const Camera3D& camera) {
	// NOTE(JOE): this must be drawn in 2D mode, not 3D mode

	// Currently this just draw the text independent of zoom.
	// It also seems to be located at the top left, so there should be an offset
	// based on the font size and the camera zoom should be taken into account somehow.

	for (int i = 0; i < atoms.natoms; i++) {
		Vector2 pos = GetWorldToScreen(atoms.xyz[i], camera);
		// SPEED: Need a faster way of testing for overlapping spheres to not draw background number
		// Also, this isn't really a very important problem for most use cases.
		//int collisionIndex = model.TestRayAgainst(GetMouseRay(pos, camera));
		//if (collisionIndex != -1 && collisionIndex == i) {
			DrawText(std::to_string(i+1).c_str(), (int)pos.x, (int)pos.y, 14, GREEN);//ContrastingColor(model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color));
		//}
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

////////////////////////////////////
////// WIDGET DRAWING METHODS //////
////////////////////////////////////

//void Draw3DAxes(const Vector3& point) {
//	Material material = LoadMaterialDefault();
//	material.maps[MATERIAL_MAP_DIFFUSE].color = GREEN;
//	// Draw cylinder of 3D arrow
//	Mesh cylinderMesh = GenMeshCylinder(0.1, 1.0, 24);
//	Mesh coneMesh = GenMeshCone(0.2, 0.25, 24);
//	// Y axis
//	DrawMesh(cylinderMesh, material, MatrixTranslate(point));
//	DrawMesh(coneMesh, material, MatrixTranslate(point + (Vector3){0.0f, 1.0f, 0.0f}));
//	// Z axis
//	material.maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
//	DrawMesh(cylinderMesh, material, MatrixTranslate(point) * MatrixAlignToAxis((Vector3){0,1,0}, (Vector3){0,0,1}));
//	DrawMesh(coneMesh, material, MatrixTranslate(point + (Vector3){0.0f, 1.0f, 0.0f}) * MatrixAlignToAxis((Vector3){0,1,0}, (Vector3){0,0,1}));
//	// X axis
//	material.maps[MATERIAL_MAP_DIFFUSE].color = RED;
//	DrawMesh(cylinderMesh, material, MatrixTranslate(point) * MatrixAlignToAxis((Vector3){0,1,0}, (Vector3){1,0,0}));
//	DrawMesh(coneMesh, material, MatrixTranslate(point + (Vector3){0.0f, 1.0f, 0.0f}) * MatrixAlignToAxis((Vector3){0,1,0}, (Vector3){1,0,0}));
//}