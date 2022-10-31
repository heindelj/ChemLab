
void NewFrame(ActiveContext& context) {
	context.frames->nframes++;
	Atoms newAtoms = Atoms();
	newAtoms.natoms = 0;
	context.frames->atoms.push_back(newAtoms);
	context.frames->headers.push_back("");
	context.activeFrame = context.frames->nframes - 1;
	context.renderContext.model = BallAndStickModelFromAtoms(
		context.frames->atoms[context.activeFrame],
		&context.renderContext.lightingShader
		);
}

void AddAtom(ActiveContext& context, const std::string& newAtomLabel, const Vector3& position) {
	context.frames->atoms[context.activeFrame].natoms++;
	context.frames->atoms[context.activeFrame].xyz.push_back(position);
	context.frames->atoms[context.activeFrame].labels.push_back(newAtomLabel);
	context.frames->atoms[context.activeFrame].renderData.push_back(GetRenderData(newAtomLabel));
	context.indicesBeingAdded.push_back(context.frames->atoms[context.activeFrame].natoms - 1);

	// TODO: Update the material to have an opacity or maybe be highlighted?

	context.renderContext.model = BallAndStickModelFromAtoms(
		context.frames->atoms[context.activeFrame],
		&context.renderContext.lightingShader
		);
}

void StopAddingAtoms(ActiveContext& context) {
	context.addingNewAtoms = false;
	context.lockCamera = false;
	context.indicesBeingAdded.clear();
}

void BuildZMatrix(ActiveContext& context) {
	if (!context.addingNewAtoms) {
		//debug("Add an atom please.");
		// should display a dialog message instructing to add an atom
		return;
	}

	// TODO: Display a dialog message at each step with help instructions
	switch(context.selectionStep) {
		case NONE: {
			// should display a dialog message instructing to add an atom
			break;
		}
		case DISTANCE: {
			// handle the distance
			break;
		}
		case ANGLE: {
			break;
		}
		case DIHEDRAL: {
			break;
		}
	}
}

bool IsValidZMatrixInput(const ActiveContext& context, SelectionStep step) {
	// check that none of the indices for distance, angle, and dihedral are identical
	if (step == ANGLE) {
		if (context.distanceIndex == context.angleIndex) {
			// TODO: Display an error message saying that two of the selected indices are identical
			return false;
		}
	}
	else if (step == DIHEDRAL) {
		if ((context.distanceIndex == context.angleIndex) || (context.distanceIndex == context.dihedralIndex) || (context.angleIndex == context.dihedralIndex)) {
			return false;
		}

	}
	// check the new atoms are not collinear
	//if (context.zmat->coordinates.size() > 3) {
	//	// TODO: Make sure no bond angles are 180
	//}
	return true;
}

void AtomPositionFromZMatrix(ActiveContext& context) {
	// Note: We always decrement the index for distance, angle, and dihedral by 1 since the user
	// is picking based off of the number, not the zero-based index.
	int activeAtomIndex = context.frames->atoms[context.activeFrame].xyz.size() - 1;
	switch(activeAtomIndex) {
		case 1: { // adding second atom
			context.frames->atoms[context.activeFrame].xyz[activeAtomIndex] = (Vector3){context.distanceSliderValue, 0.0f, 0.0f};
			OnAtomMove(context.frames->atoms[context.activeFrame], activeAtomIndex, *context.renderContext.model);
			break;
		}
		case 2: { // adding third atom using help from: https://math.stackexchange.com/questions/2156851/calculate-the-coordinates-of-the-third-vertex-of-triangle-given-the-other-two-an
			if (IsValidZMatrixInput(context, ANGLE)) {
				// Find position of atom C from the the AB positions using some trig
				// Get third side length from law of cosines
				std::cout << context.distanceIndex << std::endl;
				Vector3 bondAB = context.frames->atoms[context.activeFrame].xyz[context.distanceIndex-1] - context.frames->atoms[context.activeFrame].xyz[context.angleIndex-1];
				float l1 = norm(bondAB);
				float l2 = context.distanceSliderValue;
				float l3 = sqrt(l1 * l1 + l2 * l2 - 2 * l1 * l2 * cos(PI - context.angleSliderValue * DEG2RAD));
				float phi1 = atan2(bondAB.y - 0, bondAB.x - 0);
				float phi2 = acos((l1 * l1 + l2 * l2 - l3 * l3) / (2 * l1 * l2));
				float Cx = bondAB.x + l2 * cos(phi1 + phi2);
				float Cy = bondAB.y + l2 * sin(phi1 + phi2);

				context.frames->atoms[context.activeFrame].xyz[activeAtomIndex] = context.frames->atoms[context.activeFrame].xyz[context.angleIndex-1];
				context.frames->atoms[context.activeFrame].xyz[activeAtomIndex].x += Cx;
				context.frames->atoms[context.activeFrame].xyz[activeAtomIndex].y += Cy;

				OnAtomMove(context.frames->atoms[context.activeFrame], std::vector<int>{activeAtomIndex, context.distanceIndex-1}, *context.renderContext.model);
			} else {
				std::cout << "Invalid indices" << std::endl;
			}
			break;
		}
		case 3: { // adding fourth or greater atom
			break;
		}
		default: { // adding first atom
			break;
		}
	}
}

void DrawBuildUI(ActiveContext& context) {
	// Draw text label for settings
	const char* title = "SETTINGS";
	int fontSize = 30;
	DrawText(title, context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2, (context.screenHeight - context.uiSettings.borderWidth) / 2, fontSize, BLACK);
	DrawRectangle(context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2 - 5, (context.screenHeight - context.uiSettings.borderWidth) / 2 + fontSize, MeasureText(title, fontSize) + 10, 5, BLACK); // underline text
	
	GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
	// Add Frame Button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("ADD FRAME", 12) / 2, 60, (float)MeasureText("ADD FRAME", 12), 30.0f}, "ADD FRAME")) {
		NewFrame(context);
	}
	// New Z-Matrix Button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("NEW Z-MATRIX", 12) / 2, 180, (float)MeasureText("NEW Z-MATRIX", 12), 30.0f}, "NEW Z-MATRIX")) {
		if (context.frames->atoms[context.activeFrame].natoms != 0) {
			NewFrame(context);
		}
		context.buildingZMatrix = true;
		context.zmat = new ZMatrix();
		context.zmat->transform = MatrixIdentity();
		AddAtom(context, "H", Vector3Zero());
	}
	// ZMatrix UI
	if (context.buildingZMatrix) {
		// TODO: Add a spinner box for selecting which atom is actually being edited. Might have to delete all other atoms?
		int activeAtomIndex = context.frames->atoms[context.activeFrame].xyz.size() - 1;
		// Distance slider
		context.distanceSliderValue = GuiSlider((Rectangle){ context.uiSettings.menuWidth - 125, 240, 100, 20}, "DISTANCE", TextFormat("%2.3f", (float)context.distanceSliderValue), context.distanceSliderValue, 0, 10);
		// Angle slider
		context.angleSliderValue = GuiSlider((Rectangle){ context.uiSettings.menuWidth - 125, 270, 100, 20}, "ANGLE", TextFormat("%2.1f", (float)context.angleSliderValue), context.angleSliderValue, 1, 179);
		// Dihedral slider
		context.dihedralSliderValue = GuiSlider((Rectangle){ context.uiSettings.menuWidth - 125, 300, 100, 20}, "DIHEDRAL", TextFormat("%2.1f", (float)context.dihedralSliderValue), context.dihedralSliderValue, 0, 180);
		
		// distance index selection
		if (GuiSpinner((Rectangle){ context.uiSettings.borderWidth + 5, 240, 80, 20}, NULL, &context.distanceIndex, 1, context.zmat->coordinates.size(), context.spinnerEditMode)) {
			context.spinnerEditMode = !context.spinnerEditMode;
		}
		// angle index selection
		if (GuiSpinner((Rectangle){ context.uiSettings.borderWidth + 5, 270, 80, 20}, NULL, &context.angleIndex, 1, context.zmat->coordinates.size(), context.spinnerEditMode)) {
			context.spinnerEditMode = !context.spinnerEditMode;
		}
		// dihedral index selection
		if (GuiSpinner((Rectangle){ context.uiSettings.borderWidth + 5, 300, 80, 20}, NULL, &context.dihedralIndex, 1, context.zmat->coordinates.size(), context.spinnerEditMode)) {
			context.spinnerEditMode = !context.spinnerEditMode;
		}

		// Update position of active atom based on the slider values and index selections
		AtomPositionFromZMatrix(context);

		// Button for accepting slider values and adding atom to active frame
		// TODO: It's possible to just eliminate the zmatrix data structure from all of this.
		// We should still store the various angles and indices because being able to export the z-matrix is convenient.
		if (GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - 50 - 20, 330, (float)MeasureText("ACCEPT PLACEMENT", 12), 30}, "ACCEPT PLACEMENT")) {
			switch(context.zmat->coordinates.size()) {
				case 0: {
					context.zmat->coordinates.push_back((std::array<float,3>){0.0f, 0.0f, 0.0f});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){-1, -1, -1});
					AddAtom(context, "H", (Vector3){1.0f, 0.0f, 0.0f});
					break;
				}
				case 1: {
					context.zmat->coordinates.push_back((std::array<float,3>){context.distanceSliderValue, 0.0f, 0.0f});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){context.distanceIndex-1, -1, -1});
					AddAtom(context, "H", context.frames->atoms[context.activeFrame].xyz[context.frames->atoms[context.activeFrame].natoms-1] + (Vector3){1.0f, 0.0f, 0.0f});
					break;
				}
				case 2: {
					context.zmat->coordinates.push_back((std::array<float,3>){context.distanceSliderValue, context.angleSliderValue, 0.0f});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){context.distanceIndex-1, context.angleIndex-1, -1});
					break;
				}
				default: {
					context.zmat->coordinates.push_back((std::array<float,3>){context.distanceSliderValue, context.angleSliderValue, context.dihedralSliderValue});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){context.distanceIndex-1, context.angleIndex-1, context.dihedralIndex-1});
					break;
				}
			}
		}
	}
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("ADD ATOM", 12) / 2, 120, (float)MeasureText("ADD ATOM", 12), 30.0f}, "ADD ATOM")) {
		if (!context.addingNewAtoms) {
			context.addingNewAtoms = true;
			AddAtom(context, "H", (Vector3){0,0,0});
			context.lockCamera = true;
	
			// Set the mouse at the center of the screen so you are immediately moving the atom where you want it to go.
			// For multi-atom selections, need to spawn the centroid at the center of the screen or move the mouse to the
			// centroid of the model wherever it gets placed.
			SetMousePosition(context.screenWidth / 2, context.screenHeight / 2);
		} else {
			// TODO: Also delete the atom that was added already
			StopAddingAtoms(context);
		}
	}

	// print xyz coordinates button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("PRINT XYZ", 12) / 2, (float)context.screenHeight - 100, (float)MeasureText("PRINT XYZ", 12), 30.0f}, "PRINT XYZ")) {
		const std::string xyzString = GetXYZFormattedString(context.frames->atoms[context.activeFrame]);
		std::cout << xyzString << std::endl;
	}	
}

void HighlightAtomOnShiftClick(ActiveContext& context, int collisionIndex) {
	// Check if shift-clicking to highlight atom
	// Copy and pasted from view mode.
	if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
		if (collisionIndex != -1) {
			if (context.atomsToHighlight.count(collisionIndex)) {
				context.atomsToHighlight.erase(collisionIndex);
			} else {
				context.atomsToHighlight.insert(collisionIndex);
			}
		} 
	}
}

RayCollision CollisionWithPlane(ActiveContext& context, const Vector3& point, const Vector3& axis) {
	// Returns collision info with a plane, defined by a point on the plane and the plane normal,
	// by casting a ray from the mouse position.
	Vector3 n1 = normalize(cross(normalize(point), normalize(axis)));
	Vector3 n2 = normalize(cross(n1, axis));
	// just check collision with quad with vertices way far apart for getting poisition on plane.
	RayCollision planeHitInfo = GetRayCollisionQuad(GetMouseRay(GetMousePosition(), context.renderContext.camera), 
		point - 1000 * axis, point + 1000 * axis, point + 1000 * n1, point - 1000 * n1);

	if (planeHitInfo.hit && norm(context.lastPoint) < 1e-7) {
		context.lastPoint = planeHitInfo.point;
	}
	return planeHitInfo;
}

void EditActiveFrame(ActiveContext& context) {
	// Used for editing an active molecular frame. Logic for translating and rotating a selection
	// of atoms. Also manages setting the internal vs. cartesian editing modes.

	if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		context.lockCamera = false;
		context.isAxisSelected = false;
		context.lastPoint = Vector3Zero();

		double timeSinceClick = GetTimeSinceClick(context);
		int collisionIndex = context.renderContext.model->TestRayAgainst(GetMouseRay(GetMousePosition(), context.renderContext.camera));
		
		HighlightAtomOnShiftClick(context, collisionIndex);

		// check for collision with nothing
		if (collisionIndex == -1 && timeSinceClick <= 0.5)
			context.atomsToHighlight.clear();
	} else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && context.atomsToHighlight.size() != 0) {
		// If there are axes being drawn and we are holding down the left mouse button,
		// then check if we are clicking on one of the axes.
		if (!context.isAxisSelected) {
			context.activeAxis = context.axes->TestRayAgainst(GetMouseRay(GetMousePosition(), context.renderContext.camera));
		}
		if (context.activeAxis != -1) { // We're clicking on an axis
			context.lockCamera = true;
			context.isAxisSelected = true;

			Vector3 selectionCentroid = centroid(context.frames->atoms[context.activeFrame].xyz, context.atomsToHighlight);
			Vector3 axis = Vector3Zero();
			if (context.activeAxis == 0) { // X-Axis
				axis = (Vector3){1, 0, 0};
				DrawLine3D(selectionCentroid, selectionCentroid - 1000 * axis, RED);
				DrawLine3D(selectionCentroid, selectionCentroid + 1000 * axis, RED);
			} else if (context.activeAxis == 1) { // Y-Axis
				axis = (Vector3){0, 1, 0};
				DrawLine3D(selectionCentroid, selectionCentroid - 1000 * axis, GREEN);
				DrawLine3D(selectionCentroid, selectionCentroid + 1000 * axis, GREEN);
			} else if (context.activeAxis == 2) { // Z-Axis
				axis = (Vector3){0, 0, 1};
				DrawLine3D(selectionCentroid, selectionCentroid - 1000 * axis, BLUE);
				DrawLine3D(selectionCentroid, selectionCentroid + 1000 * axis, BLUE);
			}
			RayCollision planeHitInfo = CollisionWithPlane(context, selectionCentroid, axis);
			
			// The whole planhitinfo thing is pretty scuffed and should be improved.
			for (int i : context.atomsToHighlight) {
				context.frames->atoms[context.activeFrame].xyz[i] += (planeHitInfo.point - context.lastPoint) * axis;
			}
			context.lastPoint = planeHitInfo.point;
			OnAtomMove(context.frames->atoms[context.activeFrame], context.atomsToHighlight, *context.renderContext.model);
		}
	}

	if (context.atomsToHighlight.size() != 0) {
		// Should try to figure out which axis is in front and modify draw order of the axes.
		//rlDisableDepthTest();
		const Vector3 axesPosition = centroid(context.frames->atoms[context.activeFrame].xyz, context.atomsToHighlight);
		context.axes->Draw(axesPosition);
		//rlEnableDepthTest();
	}


}

void BuildModeFrame(ActiveContext& context) {
	ClearBackground(context.renderContext.backgroundColor);
	BeginMode3D(context.renderContext.camera);
		context.renderContext.model->DrawHighlighted(context.atomsToHighlight);
	    context.renderContext.model->Draw();

	    if (context.drawGrid)
	    	DrawGrid(10, 1.0f);
	
	if(context.addingNewAtoms && !context.buildingZMatrix) {
		DrawPlane(context.gridModel, context.renderContext.camera.position - context.renderContext.camera.target, context.renderContext.camera.target);
		RayCollision hitInfo = GetRayCollisionModel(GetMouseRay(GetMousePosition(), context.renderContext.camera), context.gridModel);

		for (auto it = context.indicesBeingAdded.begin(); it != context.indicesBeingAdded.end(); ++it) {
			if (hitInfo.hit)
				context.frames->atoms[context.activeFrame].xyz[*it] = hitInfo.point;
		}
		OnAtomMove(context.frames->atoms[context.activeFrame], context.indicesBeingAdded, *context.renderContext.model);
		if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && GetMouseX() > (context.uiSettings.menuWidth + context.uiSettings.borderWidth)) {
			StopAddingAtoms(context);
		}
	}
	else if (context.buildingZMatrix) {
		BuildZMatrix(context);
	} else {
		EditActiveFrame(context);
	}

	EndMode3D();
}
