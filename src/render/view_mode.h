#pragma once

int NumberOfValidIndices(std::array<int, 4> arr) {
	int count = 0;
	for (int i = 0; i < arr.size(); i++) {
		if (arr[i] >= 0)
			count += 1;
	}
	return count;
}

void ResetViewSelection(ActiveContext& context) {
	context.selectionStep = NONE;
	context.viewSelection.fill(-1);
}

double GetTimeSinceClick(ActiveContext& context) {
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;
	return timeSinceClick;
}

void OnClickReleaseViewNone(MolecularModel& model, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 0);
	double timeSinceClick = GetTimeSinceClick(context);

	if (timeSinceClick <= 0.5) {
		int collisionIndex = model.TestRayAgainst(GetMouseRay(GetMousePosition(), context.camera));
		if (collisionIndex != -1) {
			// Store collision index for future use in displaying geometric info.
			context.selectionStep = DISTANCE;
			context.viewSelection[0] = collisionIndex;
		} else {
			context.permanentSelection.clear();
		}
	}

}

void OnClickReleaseViewDistance(MolecularModel& model, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 1);
	double timeSinceClick = GetTimeSinceClick(context);

	int collisionIndex = model.TestRayAgainst(GetMouseRay(GetMousePosition(), context.camera));
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0]) {
		context.viewSelection[1] = collisionIndex;
		context.selectionStep = ANGLE;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewAngle(MolecularModel& model, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 2);
	double timeSinceClick = GetTimeSinceClick(context);

	int collisionIndex = model.TestRayAgainst(GetMouseRay(GetMousePosition(), context.camera));
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1]) {
		context.viewSelection [2] = collisionIndex;
		context.selectionStep = DIHEDRAL;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewDihedral(MolecularModel& model, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 3);
	double timeSinceClick = GetTimeSinceClick(context);

	int collisionIndex = model.TestRayAgainst(GetMouseRay(GetMousePosition(), context.camera));
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1] && collisionIndex != context.viewSelection[2]) {
		context.viewSelection[3] = collisionIndex;
		context.permanentSelection.push_back(context.viewSelection);
		ResetViewSelection(context);
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void ViewModeFrame(MolecularModel& model, ActiveContext& context) {

	BeginMode3D(context.camera);
	    model.Draw();
	    DrawGrid(10, 1.0f);
	EndMode3D();

	// handle mouse input based on selection step
	if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		switch(context.selectionStep) {
			case NONE :
				OnClickReleaseViewNone(model, context);
				break;
			case DISTANCE :
				OnClickReleaseViewDistance(model, context);
				break;
			case ANGLE :
				OnClickReleaseViewAngle(model, context);
				break;
			case DIHEDRAL :
				OnClickReleaseViewDihedral(model, context);
				break;
		}
	}

	// TODO: I somehow need to change the draw order to make the lines
	// be drawn on top of the spheres.

	// draw lines and geometry labels based on selections step.
	switch(context.selectionStep) {
		case NONE :
			break;
		case DISTANCE :
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[0]]), context.camera), context.lineWidth, PURPLE);
			break;
		case ANGLE :
			// Also need to draw the distance label
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[1]]), context.camera), context.lineWidth, PURPLE);
			DrawLineBetweenPoints(model, context.viewSelection[0], context.viewSelection[1], context.camera, context.lineWidth, PURPLE);
			break;
		case DIHEDRAL :
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[2]]), context.camera), context.lineWidth, PURPLE);
			DrawLineBetweenPoints(model, context.viewSelection[0], context.viewSelection[1], context.camera, context.lineWidth, PURPLE);
			DrawLineBetweenPoints(model, context.viewSelection[1], context.viewSelection[2], context.camera, context.lineWidth, PURPLE);
			break;
	}

	// Draw any completed selections we've stored
	for (int i = 0; i < context.permanentSelection.size(); i++) {
		DrawLineBetweenPoints(model, context.permanentSelection[i][0], context.permanentSelection[i][1], context.camera, context.lineWidth, GREEN);
		DrawLineBetweenPoints(model, context.permanentSelection[i][1], context.permanentSelection[i][2], context.camera, context.lineWidth, GREEN);
		DrawLineBetweenPoints(model, context.permanentSelection[i][2], context.permanentSelection[i][3], context.camera, context.lineWidth, GREEN);
	}
}