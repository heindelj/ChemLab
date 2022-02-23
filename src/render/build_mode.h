
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

bool IsValidZMatrixInput(const ActiveContext& context) {
	// check that none of the indices for distance, angle, and dihedral are identical
	if (!((context.spinnerValue1 != context.spinnerValue2) && (context.spinnerValue1 != context.spinnerValue3) && (context.spinnerValue3 != context.spinnerValue3))) {
		// TODO: Display an error message saying that two of the selected indices are identical
		return false;
	}
	// check the new atoms are not collinear
	if (context.zmat->coordinates.size() > 3) {
		// TODO: Make sure no bond angles are 180
	}
	return true;
}

void DrawBuildUI(ActiveContext& context) {
	// Draw text label for settings
	const char* title = "SETTINGS";
	int fontSize = 30;
	DrawText(title, context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2, (context.screenHeight - context.uiSettings.borderWidth) / 2, fontSize, BLACK);
	DrawRectangle(context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2 - 5, (context.screenHeight - context.uiSettings.borderWidth) / 2 + fontSize, MeasureText(title, fontSize) + 10, 5, BLACK); // underline text
	
	GuiSetStyle(BUTTON, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
	// Add Frame Button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("ADD FRAME", 12) / 2, 60, (float)MeasureText("ADD FRAME", 12), 30.0f}, "ADD FRAME")) {
		NewFrame(context);
	}
	// New Z-Matrix Button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("NEW Z-MATRIX", 12) / 2, 180, (float)MeasureText("NEW Z-MATRIX", 12), 30.0f}, "NEW Z-MATRIX")) {
		context.buildingZMatrix = true;
		context.zmat = new ZMatrix();
		context.zmat->transform = MatrixIdentity();
	}
	// ZMatrix UI
	if (context.buildingZMatrix) {
		// TODO: Add a spinner box for selecting which atom is actually being edited. Might have to delete all other atoms?
		int activeAtomIndex = context.zmat->coordinates.size();
		// Distance slider
		context.distanceSliderValue = GuiSlider((Rectangle){ context.uiSettings.menuWidth - 125, 240, 100, 20}, "DISTANCE", TextFormat("%2.3f", (float)context.distanceSliderValue), context.distanceSliderValue, 0, 10);
		// Angle slider
		context.angleSliderValue = GuiSlider((Rectangle){ context.uiSettings.menuWidth - 125, 270, 100, 20}, "ANGLE", TextFormat("%2.1f", (float)context.angleSliderValue), context.angleSliderValue, 0, 180);
		// Dihedral slider
		context.dihedralSliderValue = GuiSlider((Rectangle){ context.uiSettings.menuWidth - 125, 300, 100, 20}, "DIHEDRAL", TextFormat("%2.1f", (float)context.dihedralSliderValue), context.dihedralSliderValue, 0, 180);
		
		// distance index selection
		if (GuiSpinner((Rectangle){ context.uiSettings.borderWidth + 5, 240, 80, 20}, NULL, context.spinnerValue1, 0, context.zmat->coordinates.size()-1, context.spinnerEditMode)) {
			context.spinnerEditMode = !context.spinnerEditMode;
		}
		// angle index selection
		if (GuiSpinner((Rectangle){ context.uiSettings.borderWidth + 5, 270, 80, 20}, NULL, context.spinnerValue2, 0, context.zmat->coordinates.size()-1, context.spinnerEditMode)) {
			context.spinnerEditMode = !context.spinnerEditMode;
		}
		// dihedral index selection
		if (GuiSpinner((Rectangle){ context.uiSettings.borderWidth + 5, 300, 80, 20}, NULL, context.spinnerValue3, 0, context.zmat->coordinates.size()-1, context.spinnerEditMode)) {
			context.spinnerEditMode = !context.spinnerEditMode;
		}

		// Button for accepting slider values and adding atom to active frame
		if (GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - 50 - 20, 330, (float)MeasureText("ACCEPT PLACEMENT", 12), 30}, "ACCEPT PLACEMENT")) {
			switch(context.zmat->coordinates.size()) {
				case 0: {
					context.zmat->coordinates.push_back((std::array<float,3>){0.0f, 0.0f, 0.0f});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){-1, -1, -1});
				}
				case 1: {
					context.zmat->coordinates.push_back((std::array<float,3>){distanceSliderValue, 0.0f, 0.0f});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){context.spinnerValue1, -1, -1});
				}
				case 2: {
					context.zmat->coordinates.push_back((std::array<float,3>){distanceSliderValue, angleSliderValue, 0.0f});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){context.spinnerValue1, context.spinnerValue2, -1});
				}
				default: {
					context.zmat->coordinates.push_back((std::array<float,3>){distanceSliderValue, angleSliderValue, dihedralSliderValue});
					context.zmat->coordinateIndices.push_back((std::array<int,3>){context.spinnerValue1, context.spinnerValue2, context.spinnerValue3});
				}
			}
			// TODO: Add the new atom to the frame!!!!
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
	}

	EndMode3D();
}