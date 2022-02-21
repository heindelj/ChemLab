
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

	// TODO: Update the material to have an opacity!

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

void DrawBuildUI(ActiveContext& context) {

	// Draw text label for settings
	const char* title = "SETTINGS";
	const int fontSize = 30;
	DrawText(title, context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2, (context.screenHeight - context.uiSettings.borderWidth) / 2, fontSize, BLACK);
	DrawRectangle(context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2 - 5, (context.screenHeight - context.uiSettings.borderWidth) / 2 + fontSize, MeasureText(title, fontSize) + 10, 5, BLACK); // underline text
	
	GuiSetStyle(BUTTON, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
	// Add Frame Button
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("ADD FRAME", 12) / 2, 60, (float)MeasureText("ADD FRAME", 12), 30.0f}, "ADD FRAME")) {
		NewFrame(context);
	}
	if(GuiButton((Rectangle){ context.uiSettings.menuWidth / 2 - (float)MeasureText("ADD ATOM", 12) / 2, 120, (float)MeasureText("ADD ATOM", 12), 30.0f}, "ADD ATOM")) {
		debug(context.addingNewAtoms);
		if (!context.addingNewAtoms) {
			context.addingNewAtoms = true;
			AddAtom(context, "H", (Vector3){0,0,0});
			context.lockCamera = true;
	
			// Set the mouse at the center of the screen so you are immediately moving the atom where you want it to go.
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
	
	if(context.addingNewAtoms) {
		// Currently this is working except I need to add ray-plane intersection with the plane I want to move
		// the atom in. I have to implement this myself I think which should be easy. Then, I can move the 
		// atom on that plane and place it as I please.
		// Or maybe we just move along the camera right vector???
		Vector3 cameraRight = cross(normalize(context.renderContext.camera.target - context.renderContext.camera.position), context.renderContext.camera.up);
		DrawPlane(context.gridModel, context.renderContext.camera.position - context.renderContext.camera.target, context.renderContext.camera.target);
		RayCollision hitInfo = GetRayCollisionModel(GetMouseRay(GetMousePosition(), context.renderContext.camera), context.gridModel);

		for (auto it = context.indicesBeingAdded.begin(); it != context.indicesBeingAdded.end(); ++it) {
			if (hitInfo.hit)
				context.frames->atoms[context.activeFrame].xyz[*it] = hitInfo.point;
		}
		OnAtomMove(context.frames->atoms[context.activeFrame], context.indicesBeingAdded, *context.renderContext.model);
		if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && GetMouseX() > (context.uiSettings.menuWidth + context.uiSettings.borderWidth)) {
			StopAddingAtoms(context);
			// TODO: Remove the opacity from the material on the atom that was just placed.
		}
	}
	EndMode3D();
}