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

void OnClickReleaseViewNone(Atoms& atoms, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 0);
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;

	if (timeSinceClick <= 0.5) {
		int collisionIndex = TestRayAgainstAtoms(GetMouseRay(GetMousePosition(), context.camera), atoms);
		if (collisionIndex != -1) {
			// Store collision index for future use in displaying geometric info.
			context.selectionStep = DISTANCE;
			context.viewSelection[0] = collisionIndex;
		} else {
			context.permanentSelection.clear();
		}
	}

}

void OnClickReleaseViewDistance(Atoms& atoms, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 1);
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;

	int collisionIndex = TestRayAgainstAtoms(GetMouseRay(GetMousePosition(), context.camera), atoms);
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0]) {
		context.viewSelection[1] = collisionIndex;
		context.selectionStep = ANGLE;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewAngle(Atoms& atoms, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 2);
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;

	int collisionIndex = TestRayAgainstAtoms(GetMouseRay(GetMousePosition(), context.camera), atoms);
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1]) {
		context.viewSelection [2] = collisionIndex;
		context.selectionStep = DIHEDRAL;
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void OnClickReleaseViewDihedral(Atoms& atoms, ActiveContext& context) {
	assert(NumberOfValidIndices(context.viewSelection) == 3);
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;

	int collisionIndex = TestRayAgainstAtoms(GetMouseRay(GetMousePosition(), context.camera), atoms);
	if (collisionIndex != -1 && collisionIndex != context.viewSelection[0] && collisionIndex != context.viewSelection[1] && collisionIndex != context.viewSelection[2]) {
		context.viewSelection[3] = collisionIndex;
		context.permanentSelection.push_back(context.viewSelection);
		ResetViewSelection(context);
	} else if (timeSinceClick <= 0.5) {
		ResetViewSelection(context);
	}
}

void ViewModeFrame(Atoms& atoms, ActiveContext& context) {

	BeginMode3D(context.camera);
	    DrawAtoms(atoms, context.style);
	    DrawGrid(10, 1.0f);
	EndMode3D();

	// handle mouse input based on selection step
	if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		switch(context.selectionStep) {
			case NONE :
				OnClickReleaseViewNone(atoms, context);
				break;
			case DISTANCE :
				OnClickReleaseViewDistance(atoms, context);
				break;
			case ANGLE :
				OnClickReleaseViewAngle(atoms, context);
				break;
			case DIHEDRAL :
				OnClickReleaseViewDihedral(atoms, context);
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
			DrawDashedLineFromPointToCursor(GetWorldToScreen(atoms.xyz[context.viewSelection[0]], context.camera), PURPLE);
			break;
		case ANGLE :
			// Also need to draw the distance label
			DrawDashedLineFromPointToCursor(GetWorldToScreen(atoms.xyz[context.viewSelection[1]], context.camera), PURPLE);
			DrawLineBetweenAtoms(atoms, context.viewSelection[0], context.viewSelection[1], context.camera, PURPLE);
			break;
		case DIHEDRAL :
			DrawDashedLineFromPointToCursor(GetWorldToScreen(atoms.xyz[context.viewSelection[2]], context.camera), PURPLE);
			DrawLineBetweenAtoms(atoms, context.viewSelection[0], context.viewSelection[1], context.camera, PURPLE);
			DrawLineBetweenAtoms(atoms, context.viewSelection[1], context.viewSelection[2], context.camera, PURPLE);
			break;
	}

	// Draw any completed selections we've stored
	for (int i = 0; i < context.permanentSelection.size(); i++) {
		DrawLineBetweenAtoms(atoms, context.permanentSelection[i][0], context.permanentSelection[i][1], context.camera, RAYWHITE);
		DrawLineBetweenAtoms(atoms, context.permanentSelection[i][1], context.permanentSelection[i][2], context.camera, RAYWHITE);
		DrawLineBetweenAtoms(atoms, context.permanentSelection[i][2], context.permanentSelection[i][3], context.camera, RAYWHITE);
	}
	rlEnableDepthTest();
}