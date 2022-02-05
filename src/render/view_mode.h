#pragma once

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
	if (context.isRotating) {
		context.forwardOnStartingToRotate = normalize(context.renderContext.camera.target - context.renderContext.camera.position);
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

void OnClickReleaseViewNone(MolecularModel& model, ActiveContext& context, int collisionIndex) {
	assert(NumberOfValidIndices(context.viewSelection) == 0);
	double timeSinceClick = GetTimeSinceClick(context);

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

void OnClickReleaseViewDistance(MolecularModel& model, ActiveContext& context, int collisionIndex) {
	assert(NumberOfValidIndices(context.viewSelection) == 1);
	double timeSinceClick = GetTimeSinceClick(context);
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0]) {
		context.viewSelection[1] = collisionIndex;
		context.selectionStep = ANGLE;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewAngle(MolecularModel& model, ActiveContext& context, int collisionIndex) {
	assert(NumberOfValidIndices(context.viewSelection) == 2);
	double timeSinceClick = GetTimeSinceClick(context);
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1]) {
		context.viewSelection [2] = collisionIndex;
		context.selectionStep = DIHEDRAL;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewDihedral(MolecularModel& model, ActiveContext& context, int collisionIndex) {
	assert(NumberOfValidIndices(context.viewSelection) == 3);
	double timeSinceClick = GetTimeSinceClick(context);
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

void HandleSelections(MolecularModel& model, ActiveContext& context) {
	// handle mouse input based on selection step
	if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		int collisionIndex = model.TestRayAgainst(GetMouseRay(GetMousePosition(), context.renderContext.camera));
		// check if shift-clicking to highlight atom
		if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
			if (collisionIndex != -1) {
				if (context.atomsToHighlight.count(collisionIndex))
					context.atomsToHighlight.erase(collisionIndex);
				else
					context.atomsToHighlight.insert(collisionIndex);
			} 
		}

		switch(context.selectionStep) {
			case NONE :
				OnClickReleaseViewNone(model, context, collisionIndex);
				break;
			case DISTANCE :
				OnClickReleaseViewDistance(model, context, collisionIndex);
				break;
			case ANGLE :
				OnClickReleaseViewAngle(model, context, collisionIndex);
				break;
			case DIHEDRAL :
				OnClickReleaseViewDihedral(model, context, collisionIndex);
				break;
		}

		// check for collision with nothing, int collisionIndex
		if (collisionIndex == -1 && !(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)))
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
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[0]]), context.renderContext.camera), context.lineWidth, YELLOW);
			break;
		}
		case ANGLE : {
			// Get distance for drawing
			DrawDistanceLineAndText(
				PositionVectorFromTransform(model.transforms[context.viewSelection[0]]), 
				PositionVectorFromTransform(model.transforms[context.viewSelection[1]]),
				YELLOW,
				context);
			
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[1]]), context.renderContext.camera), context.lineWidth, YELLOW);
			break;
		}
		case DIHEDRAL : {
			// Get vectors for calculating angle
			DrawAngleLineAndText(
				PositionVectorFromTransform(model.transforms[context.viewSelection[0]]), 
				PositionVectorFromTransform(model.transforms[context.viewSelection[1]]), 
				PositionVectorFromTransform(model.transforms[context.viewSelection[2]]), 
				YELLOW,
				context);

			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[2]]), context.renderContext.camera), context.lineWidth, YELLOW);
			break;
		}
	}

	// Draw any completed selections we've stored
	for (int i = 0; i < context.permanentSelection.size(); i++) {
		if (NumberOfValidIndices(context.permanentSelection[i]) == 2) {
			DrawDistanceLineAndText(
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][0]]),
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][1]]),
				GREEN,
				context);
		}
		if (NumberOfValidIndices(context.permanentSelection[i]) == 3) {
			DrawAngleLineAndText(
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][0]]),
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][1]]),
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][2]]),
				GREEN,
				context);
		}
		if (NumberOfValidIndices(context.permanentSelection[i]) == 4) {
			DrawDihedralLineAndText(
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][0]]),
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][1]]),
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][2]]),
				PositionVectorFromTransform(model.transforms[context.permanentSelection[i][3]]),
				GREEN,
				context);
		}
	}
}

void ViewModeFrame(MolecularModel& model, ActiveContext& context) {

	BeginMode3D(context.renderContext.camera);
		model.DrawHighlighted(context.atomsToHighlight);
	    model.Draw();
	    if (context.drawGrid)
	    	DrawGrid(10, 1.0f);
	EndMode3D();

	HandleSelections(model, context);
	
	if (context.isRotating)
		RotateAroundWorldUp(context);
	if (context.isCyclingAllFrames && (GetTime() - context.timeOfLastFrameChange) > context.timeBetweenFrameChanges) {
		context.timeOfLastFrameChange = context.timeOfLastFrameChange + (GetTime() - context.timeOfLastFrameChange);
		(context.activeFrame < context.numFrames - 1) ? context.activeFrame += 1 : context.activeFrame = 0;
		OnFrameChange(context);
	}


}