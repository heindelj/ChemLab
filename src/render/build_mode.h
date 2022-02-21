
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
		if (!context.addingNewAtoms) {
		context.addingNewAtoms = true;
		AddAtom(context, "H", (Vector3){0,0,0});
		} else { // delete the atom that the user decided not to add.

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
	EndMode3D();
	debug(context.addingNewAtoms);
	if(context.addingNewAtoms) {
		// Currently this is working except I need to add ray-plane intersection with the plane I want to move
		// the atom in. I have to implement this myself I think which should be easy. Then, I can move the 
		// atom on that plane and place it as I please.
		// Or maybe we just move along the camera right vector???
		for (auto it = context.indicesBeingAdded.begin(); it != context.indicesBeingAdded.end(); ++it) {
			Vector2 mouseDelta = GetMouseDelta();
			context.frames->atoms[context.activeFrame].xyz[*it] += (Vector3){mouseDelta.x/10, mouseDelta.y/10, 0.0f};
		}
		OnAtomMove(context.frames->atoms[context.activeFrame], context.indicesBeingAdded, *context.renderContext.model);
		if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
			context.addingNewAtoms = false;
			// TODO: Remove the opacity from the material on the atom that was just placed.
		}
	}
}