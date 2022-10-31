#pragma once

void ExportRenderTextureToImage(Image image, std::string fileName) {
	ExportImage(image, (fileName + ".png").c_str());
	UnloadImage(image);
}

Image DrawToRenderTexture(const RenderContext& renderContext, int width, int height) {
	RenderTexture2D renderTarget = LoadRenderTexture(width, height);
	BeginTextureMode(renderTarget);
	ClearBackground(Color(1.0f,1.0f,1.0f,1.0f));
	    BeginMode3D(renderContext.camera);
	    	renderContext.model->Draw();
	    EndMode3D();
	EndTextureMode();
	Image image = LoadImageFromTexture(renderTarget.texture);
	ImageFlipVertical(&image);

	// Can use this to apply a background color to exported image
	//ImageAlphaClear(&image, WHITE, 0.05f);
	UnloadRenderTexture(renderTarget);
	return image;
}

void TakeScreenshot(ActiveContext& context, int width, int height) {
	char* selectedSaveName = tinyfd_saveFileDialog("Select File Name", "screenshot.png", 0, NULL, NULL);
	if (selectedSaveName) {
		std::filesystem::path p = selectedSaveName;
		Image image = DrawToRenderTexture(context.renderContext, width, height);
		context.computeThreads.push_back(std::thread(ExportRenderTextureToImage, image, p.replace_extension()));
	}
}

void InvertSelection(ActiveContext& context) {
	std::set<int> highlightedAtoms;
	for (int i = 0; i < context.frames->atoms[context.activeFrame].natoms; ++i) {
		if (!context.atomsToHighlight.count(i))
			highlightedAtoms.insert(i);
	}
	context.atomsToHighlight = highlightedAtoms;
}

void SetHighlightedAtomColor(ActiveContext& context) {
	UpdateMaterials(context.uiSettings.colorPickerValue, context.atomsToHighlight, *context.renderContext.model);
	//for(auto itAtom = context.atomsToHighlight.begin(); itAtom != context.atomsToHighlight.end(); ++itAtom) {
	//	context.renderContext.model->materials[*itAtom].maps[MATERIAL_MAP_DIFFUSE].color = context.uiSettings.colorPickerValue;
	//	if (context.style == BALL_AND_STICK) {
	//		for(auto itStick = context.renderContext.model->stickIndices[*itAtom].begin(); itStick != context.renderContext.model->stickIndices[*itAtom].end(); ++itStick)
	//			context.renderContext.model->materials[*itStick].maps[MATERIAL_MAP_DIFFUSE].color = context.uiSettings.colorPickerValue;
	//	}
	//}
}

void SetHighlightedAtomAlpha(ActiveContext& context) {
	for(auto itAtom = context.atomsToHighlight.begin(); itAtom != context.atomsToHighlight.end(); ++itAtom) {
		context.renderContext.model->materials[*itAtom].maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(context.renderContext.model->materials[*itAtom].maps[MATERIAL_MAP_DIFFUSE].color, context.uiSettings.colorPickerAlpha);
		if (context.style == BALL_AND_STICK) {
			for(auto itStick = context.renderContext.model->stickIndices[*itAtom].begin(); itStick != context.renderContext.model->stickIndices[*itAtom].end(); ++itStick) {
				context.renderContext.model->materials[*itStick].maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(context.renderContext.model->materials[*itAtom].maps[MATERIAL_MAP_DIFFUSE].color, context.uiSettings.colorPickerAlpha);
			}
		}
	}
}

void DrawViewUI(ActiveContext& context) {

	// Draw text label for settings
	const char* title = "SETTINGS";
	const int fontSize = 30;
	DrawText(title, context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2, (context.screenHeight - context.uiSettings.borderWidth) / 2, fontSize, BLACK);
	DrawRectangle(context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2 - 5, (context.screenHeight - context.uiSettings.borderWidth) / 2 + fontSize, MeasureText(title, fontSize) + 10, 5, BLACK); // underline text

	// draw settings boxes
	float checkboxSize = 20;
	context.isCyclingAllFrames = GuiCheckBox((Rectangle){ 3 * context.uiSettings.borderWidth, (context.screenHeight - context.uiSettings.borderWidth) / 2 + 3 * fontSize, checkboxSize, checkboxSize }, "CYCLE FRAMES", context.isCyclingAllFrames);
	context.isRotating         = GuiCheckBox((Rectangle){ 3 * context.uiSettings.borderWidth, (context.screenHeight - context.uiSettings.borderWidth) / 2 + 2 * fontSize, checkboxSize, checkboxSize }, "ROTATE", context.isRotating);
	context.lockCamera         = GuiCheckBox((Rectangle){ 3 * context.uiSettings.borderWidth, (context.screenHeight - context.uiSettings.borderWidth) / 2 + 4 * fontSize, checkboxSize, checkboxSize }, "LOCK CAMERA", context.lockCamera);
	if (context.isRotating) {
		context.forwardOnStartingToRotate = normalize(context.renderContext.camera.target - context.renderContext.camera.position);
	}

	// Draw color picker and alpha picker
	context.uiSettings.colorPickerValue = GuiColorPicker((Rectangle){ 5 * context.uiSettings.borderWidth, 50, context.uiSettings.menuWidth * 0.8f, context.uiSettings.menuWidth * 0.8f}, NULL, context.uiSettings.colorPickerValue);
	context.uiSettings.colorPickerAlpha = GuiColorBarAlpha((Rectangle){ 5 * context.uiSettings.borderWidth, 280, context.uiSettings.menuWidth * 0.8f, 20}, NULL, context.uiSettings.colorPickerAlpha);
	
	GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
	if (GuiButton((Rectangle){ 50, 310, (float)MeasureText("APPLY COLOR", 12), 20}, "APPLY COLOR") && context.atomsToHighlight.size() > 0) {
		if (context.style == BALL_AND_STICK)
			SetHighlightedAtomColor(context);
	}
	if (GuiButton((Rectangle){ 150, 310, (float)MeasureText("APPLY ALPHA", 12), 20}, "APPLY ALPHA") && context.atomsToHighlight.size() > 0) {
		if (context.style == BALL_AND_STICK)
			SetHighlightedAtomAlpha(context);
	}
	if (GuiButton((Rectangle){ 50, 340, (float)MeasureText("INVERT SELECTION", 12), 20}, "INVERT SELECTION") && context.atomsToHighlight.size() > 0) {
		InvertSelection(context);
	}
	// Screenshot button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("TAKE SCREENSHOT", 12) / 2, 570, (float)MeasureText("TAKE SCREENSHOT", 12), 30.0f}, "TAKE SCREENSHOT")) {
		context.takeScreenshot = true;
	}
}

int NumberOfValidIndices(std::array<int, 4> arr) {
	int count = 0;
	for (int i = 0; i < arr.size(); i++) {
		if (arr[i] >= 0)
			count += 1;
	}
	return count;
}

double GetTimeSinceClick(ActiveContext& context) {
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;
	return timeSinceClick;
}

void OnClickReleaseViewNone(MolecularModel& model, ActiveContext& context, int collisionIndex, double timeSinceClick) {
	assert(NumberOfValidIndices(context.viewSelection) == 0);

	if (timeSinceClick <= 0.5) {
		if (collisionIndex != -1) {
			// Store collision index for future use in displaying geometric info.
			context.selectionStep = DISTANCE;
			context.viewSelection[0] = collisionIndex;
		} else {
			context.permanentSelection.clear();
		}
	}

}

void OnClickReleaseViewDistance(MolecularModel& model, ActiveContext& context, int collisionIndex, double timeSinceClick) {
	assert(NumberOfValidIndices(context.viewSelection) == 1);
	
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0]) {
		context.viewSelection[1] = collisionIndex;
		context.selectionStep = ANGLE;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewAngle(MolecularModel& model, ActiveContext& context, int collisionIndex, double timeSinceClick) {
	assert(NumberOfValidIndices(context.viewSelection) == 2);

	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1]) {
		context.viewSelection [2] = collisionIndex;
		context.selectionStep = DIHEDRAL;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewDihedral(MolecularModel& model, ActiveContext& context, int collisionIndex, double timeSinceClick) {
	assert(NumberOfValidIndices(context.viewSelection) == 3);

	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1] && collisionIndex != context.viewSelection[2]) {
		context.viewSelection[3] = collisionIndex;
		context.permanentSelection.push_back(context.viewSelection);
		ResetViewSelection(context);
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void DrawDistanceLineAndText(const Vector3& r1, const Vector3& r2, const Color& color, const ActiveContext& context) {
		float atomDistance = distance(r1, r2);

		// Get the text for distance and draw it
		char str[16];
	    sprintf(str, "%.3f", atomDistance);
		Vector2 textPos = GetWorldToScreen(midpoint(r1, r2), context.renderContext.camera);
		DrawText(str, textPos.x, textPos.y, 20, SKYBLUE);

		// Draw lines for picking next atom
		DrawLineBetweenPoints(r1, r2, context.renderContext.camera, context.lineWidth, color);
}

void DrawAngleLineAndText(const Vector3& r1, const Vector3& r2, const Vector3& r3, const Color& color, const ActiveContext& context) {
		float atomAngle = angleDeg(r1 - r2, r3 - r2);

		// Get the text for angle and draw it
		char str[16];
		sprintf(str, "%.2f", atomAngle);
		Vector2 textPos = GetWorldToScreen(midpoint(r1, r3), context.renderContext.camera);
		DrawText(str, textPos.x, textPos.y, 20, SKYBLUE);

		DrawLineBetweenPoints(r1, r2, context.renderContext.camera, context.lineWidth, color);
		DrawLineBetweenPoints(r2, r3, context.renderContext.camera, context.lineWidth, color);
}

void DrawDihedralLineAndText(const Vector3& r1, const Vector3& r2, const Vector3& r3, const Vector3& r4, const Color& color, const ActiveContext& context) {
	float dihedralAngle = dihedralDeg(r2 - r1, r3 - r2, r4 - r3);

	// Get the text for distance and draw it
	char str[16];
	sprintf(str, "%.2f", dihedralAngle);
	Vector2 textPos = GetWorldToScreen(r3 + midpoint(r2 - r1, r3 - r2), context.renderContext.camera);
	DrawText(str, textPos.x, textPos.y, 20, SKYBLUE);

	DrawLineBetweenPoints(r1, r2, context.renderContext.camera, context.lineWidth, color);
	DrawLineBetweenPoints(r2, r3, context.renderContext.camera, context.lineWidth, color);
	DrawLineBetweenPoints(r3, r4, context.renderContext.camera, context.lineWidth, color);
}

void DrawPermanentSelections(MolecularModel& model, ActiveContext& context) {
	// Draw any completed selections we've stored
	for (int i = 0; i < context.permanentSelection.size(); i++) {
		if (NumberOfValidIndices(context.permanentSelection[i]) == 2) {
			EntityIndices indices_1 = model.IDToEntityIndices[context.permanentSelection[i][0]];
			EntityIndices indices_2 = model.IDToEntityIndices[context.permanentSelection[i][1]];
			DrawDistanceLineAndText(
				PositionVectorFromTransform(model.transforms[indices_1.materialIndex][indices_1.transformIndex]),
				PositionVectorFromTransform(model.transforms[indices_2.materialIndex][indices_2.transformIndex]),
				GREEN,
				context);
		}
		if (NumberOfValidIndices(context.permanentSelection[i]) == 3) {
			EntityIndices indices_1 = model.IDToEntityIndices[context.permanentSelection[i][0]];
			EntityIndices indices_2 = model.IDToEntityIndices[context.permanentSelection[i][1]];
			EntityIndices indices_3 = model.IDToEntityIndices[context.permanentSelection[i][2]];
			DrawAngleLineAndText(
				PositionVectorFromTransform(model.transforms[indices_1.materialIndex][indices_1.transformIndex]),
				PositionVectorFromTransform(model.transforms[indices_2.materialIndex][indices_2.transformIndex]),
				PositionVectorFromTransform(model.transforms[indices_3.materialIndex][indices_3.transformIndex]),
				GREEN,
				context);
		}
		if (NumberOfValidIndices(context.permanentSelection[i]) == 4) {
			EntityIndices indices_1 = model.IDToEntityIndices[context.permanentSelection[i][0]];
			EntityIndices indices_2 = model.IDToEntityIndices[context.permanentSelection[i][1]];
			EntityIndices indices_3 = model.IDToEntityIndices[context.permanentSelection[i][2]];
			EntityIndices indices_4 = model.IDToEntityIndices[context.permanentSelection[i][3]];
			DrawDihedralLineAndText(
				PositionVectorFromTransform(model.transforms[indices_1.materialIndex][indices_1.transformIndex]),
				PositionVectorFromTransform(model.transforms[indices_2.materialIndex][indices_2.transformIndex]),
				PositionVectorFromTransform(model.transforms[indices_3.materialIndex][indices_3.transformIndex]),
				PositionVectorFromTransform(model.transforms[indices_4.materialIndex][indices_4.transformIndex]),
				GREEN,
				context);
		}
	}
}

void HandleSelections(MolecularModel& model, ActiveContext& context) {
	// handle mouse input based on selection step
	if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		double timeSinceClick = GetTimeSinceClick(context);
		int collisionIndex = model.TestRayAgainst(GetMouseRay(GetMousePosition(), context.renderContext.camera));
		// check if shift-clicking to highlight atom
		if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
			if (collisionIndex != -1) {
				if (context.atomsToHighlight.count(collisionIndex)) {
					context.atomsToHighlight.erase(collisionIndex);
				} else {
					context.atomsToHighlight.insert(collisionIndex);
					// set color picker value to highlighted atom
					context.uiSettings.colorPickerValue = context.renderContext.model->materials[model.IDToEntityIndices[collisionIndex].materialIndex].maps[MATERIAL_MAP_DIFFUSE].color;
				}
			} 
		}

		switch(context.selectionStep) {
			case NONE :
				OnClickReleaseViewNone(model, context, collisionIndex, timeSinceClick);
				break;
			case DISTANCE :
				OnClickReleaseViewDistance(model, context, collisionIndex, timeSinceClick);
				break;
			case ANGLE :
				OnClickReleaseViewAngle(model, context, collisionIndex, timeSinceClick);
				break;
			case DIHEDRAL :
				OnClickReleaseViewDihedral(model, context, collisionIndex, timeSinceClick);
				break;
		}

		// check for collision with nothing
		if (collisionIndex == -1 && timeSinceClick <= 0.5)
			context.atomsToHighlight.clear();
	}
	

	// Check if enter was pressed to store a partially complete selection
	if (IsKeyPressed(KEY_ENTER) && (context.selectionStep == ANGLE || context.selectionStep == DIHEDRAL)) {
		context.permanentSelection.push_back(context.viewSelection);
		ResetViewSelection(context);
	}

	// draw lines and geometry labels based on selections step.
	switch(context.selectionStep) {
		case NONE :
			break;
		case DISTANCE : {
			EntityIndices indices = model.IDToEntityIndices[context.viewSelection[0]];
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[indices.materialIndex][indices.transformIndex]), context.renderContext.camera), context.lineWidth, YELLOW);
			break;
		}
		case ANGLE : {
			EntityIndices indices_1 = model.IDToEntityIndices[context.viewSelection[0]];
			EntityIndices indices_2 = model.IDToEntityIndices[context.viewSelection[1]];
			DrawDistanceLineAndText(
				PositionVectorFromTransform(model.transforms[indices_1.materialIndex][indices_1.transformIndex]), 
				PositionVectorFromTransform(model.transforms[indices_2.materialIndex][indices_2.transformIndex]),
				YELLOW,
				context);
			
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[indices_2.materialIndex][indices_2.transformIndex]), context.renderContext.camera), context.lineWidth, YELLOW);
			break;
		}
		case DIHEDRAL : {
			EntityIndices indices_1 = model.IDToEntityIndices[context.viewSelection[0]];
			EntityIndices indices_2 = model.IDToEntityIndices[context.viewSelection[1]];
			EntityIndices indices_3 = model.IDToEntityIndices[context.viewSelection[2]];
			DrawAngleLineAndText(
				PositionVectorFromTransform(model.transforms[indices_1.materialIndex][indices_1.transformIndex]), 
				PositionVectorFromTransform(model.transforms[indices_2.materialIndex][indices_2.transformIndex]), 
				PositionVectorFromTransform(model.transforms[indices_3.materialIndex][indices_3.transformIndex]), 
				YELLOW,
				context);

			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[indices_3.materialIndex][indices_3.transformIndex]), context.renderContext.camera), context.lineWidth, YELLOW);
			break;
		}
	}
}

void ViewModeFrame(ActiveContext& context) {
	/////////////////////////////
	// Begin drawing functions //
	/////////////////////////////
	ClearBackground(context.renderContext.backgroundColor);
	BeginMode3D(context.renderContext.camera);
		context.renderContext.model->DrawHighlighted(context.atomsToHighlight);
	    context.renderContext.model->Draw();
	    if (context.drawGrid)
	    	DrawGrid(10, 1.0f);

	EndMode3D();
	DrawPermanentSelections(*context.renderContext.model, context);

	// Check that we arent clicking on the UI
	if ((context.drawUI == false) || GetMouseX() > (context.uiSettings.menuWidth + context.uiSettings.borderWidth))
		HandleSelections(*context.renderContext.model, context);


	///////////////////////////
	// End drawing functions //
	///////////////////////////

	///////////////////////////
	// Non-drawing functions //
	///////////////////////////
	if (context.isRotating)
		RotateAroundWorldUp(context);
	if (context.isCyclingAllFrames && (GetTime() - context.timeOfLastFrameChange) > context.timeBetweenFrameChanges) {
		context.timeOfLastFrameChange = context.timeOfLastFrameChange + (GetTime() - context.timeOfLastFrameChange);
		(context.activeFrame < context.frames->nframes - 1) ? context.activeFrame += 1 : context.activeFrame = 0;
		OnFrameChange(context);
	}
	if (context.takeScreenshot) {
		TakeScreenshot(context, 1600, 1600);
		context.takeScreenshot = false;
	}
	
	// check for changes to loaded file
	bool didUpdate = CheckForFileChangesAndUpdate(context.frames);
	if (didUpdate) {
		// TODO: Should have a setting for viewing a trajectory which can be used for playing back
		// a trajectory in real time vs. the case of editing a specific frame in an xyz file
		// which is the more common case for me, but may not be for others.

		context.frames->nframes > context.activeFrame ? context.activeFrame = context.activeFrame : context.activeFrame = context.frames->nframes - 1;
		OnFrameChange(context);
	}
}
