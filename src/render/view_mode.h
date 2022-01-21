
void OnClickReleaseView(Atoms& atoms, ActiveContext& context) {
	double timeSinceClick = GetTime() - context.timeOfLastClick;
	context.timeOfLastClick = context.timeOfLastClick + timeSinceClick;

	// seconds between which we recognize a click for selecting an atom.
	if (timeSinceClick <= 0.5) {
		int collisionIndex = TestRayAgainstAtoms(GetMouseRay(GetMousePosition(), context.camera), atoms);
		if (collisionIndex != -1) {
			// Store collision index for future use in displaying geometric info.
			context.selectionStep = DISTANCE;
			context.selection.push_back(collisionIndex);
		}
	}


	// should be getting here based on switching on the selectionStep in context.
}

void ViewModeFrame(Atoms& atoms, ActiveContext& context) {

	BeginMode3D(context.camera);
	    DrawAtoms(atoms, context.style);

	    DrawGrid(10, 1.0f);
	EndMode3D();

	if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		OnClickReleaseView(atoms, context);
	}
	switch(context.selectionStep) {
		case NONE :
			break;
		case DISTANCE :
			DrawDashedLineFromPointToCursor(GetWorldToScreen(atoms.xyz[context.selection[0]], context.camera));
	}
}