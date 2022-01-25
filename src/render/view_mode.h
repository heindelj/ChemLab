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
		case DISTANCE : {
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[0]]), context.camera), context.lineWidth, MAGENTA);
			break;
		}
		case ANGLE : {
			// Get distance for drawing
			Vector3 r1 = PositionVectorFromTransform(model.transforms[context.viewSelection[0]]);
			Vector3 r2 = PositionVectorFromTransform(model.transforms[context.viewSelection[1]]);
			float atomDistance = distance(r1, r2);

			// Get the text for distance and draw it
			char str[32];
		    sprintf(str, "%.3f", atomDistance);
			Vector2 textPos = GetWorldToScreen(midpoint(r1, r2), context.camera);
			DrawText(str, textPos.x, textPos.y, 20, SKYBLUE);

			// Draw lines for picking next atom
			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[1]]), context.camera), context.lineWidth, MAGENTA);
			DrawLineBetweenPoints(model, context.viewSelection[0], context.viewSelection[1], context.camera, context.lineWidth, MAGENTA);
			break;
		}
		case DIHEDRAL : {
			// Get vectors for calculating angle
			Vector3 r1 = PositionVectorFromTransform(model.transforms[context.viewSelection[0]]) - PositionVectorFromTransform(model.transforms[context.viewSelection[1]]);
			Vector3 r2 = PositionVectorFromTransform(model.transforms[context.viewSelection[2]]) - PositionVectorFromTransform(model.transforms[context.viewSelection[1]]);
			float atomAngle = angleDeg(r1, r2);

			// Get the text for distance and draw it
			char str[32];
		    sprintf(str, "%.2f", atomAngle);
			Vector2 textPos = GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[1]]) + midpoint(r1, r2), context.camera);
			DrawText(str, textPos.x, textPos.y, 20, SKYBLUE);

			DrawDashedLineFromPointToCursor(GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[2]]), context.camera), context.lineWidth, MAGENTA);
			DrawLineBetweenPoints(model, context.viewSelection[0], context.viewSelection[1], context.camera, context.lineWidth, MAGENTA);
			DrawLineBetweenPoints(model, context.viewSelection[1], context.viewSelection[2], context.camera, context.lineWidth, MAGENTA);
			break;
		}
	}

	// Draw any completed selections we've stored
	for (int i = 0; i < context.permanentSelection.size(); i++) {
		// Get vectors for calculating dihedral
			Vector3 r1 = PositionVectorFromTransform(model.transforms[context.permanentSelection[i][1]]) - PositionVectorFromTransform(model.transforms[context.permanentSelection[i][0]]);
			Vector3 r2 = PositionVectorFromTransform(model.transforms[context.permanentSelection[i][2]]) - PositionVectorFromTransform(model.transforms[context.permanentSelection[i][1]]);
			Vector3 r3 = PositionVectorFromTransform(model.transforms[context.permanentSelection[i][3]]) - PositionVectorFromTransform(model.transforms[context.permanentSelection[i][2]]);
			float dihedralAngle = dihedralDeg(r1, r2, r3);

			// Get the text for distance and draw it
			char str[32];
		    sprintf(str, "%.2f", dihedralAngle);
			Vector2 textPos = GetWorldToScreen(PositionVectorFromTransform(model.transforms[context.viewSelection[2]]) + midpoint(r1, r2), context.camera);
			DrawText(str, textPos.x, textPos.y, 20, SKYBLUE);

		DrawLineBetweenPoints(model, context.permanentSelection[i][0], context.permanentSelection[i][1], context.camera, context.lineWidth, GREEN);
		DrawLineBetweenPoints(model, context.permanentSelection[i][1], context.permanentSelection[i][2], context.camera, context.lineWidth, GREEN);
		DrawLineBetweenPoints(model, context.permanentSelection[i][2], context.permanentSelection[i][3], context.camera, context.lineWidth, GREEN);
	}
}