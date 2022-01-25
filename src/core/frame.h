#pragma once

void OnFrameChange(ActiveContext& context) {
	// Free the previous model
	context.model->free();	

	// Update the active model
	context.model = MolecularModelFromAtoms((*context.frames).atoms[context.activeFrame], context.style);
}

void DoFrame(ActiveContext& context) {
	bool frameChanged = CheckForFrameChange(context);
	if (frameChanged)
		OnFrameChange(context);

	switch(context.mode) {
		case VIEW:
			ViewModeFrame(*context.model, context);
			break;
		case EDIT:
			EditModeFrame(*context.model, context);
			break;
	}

	// Draw which frame number
	DrawText((std::to_string(context.activeFrame + 1) + "/" + std::to_string(context.numFrames)).c_str(), 10, 40, 20, RAYWHITE);
}