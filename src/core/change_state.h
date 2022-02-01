#pragma once

void ResetViewSelection(ActiveContext& context) {
	context.selectionStep = NONE;
	context.viewSelection.fill(-1);
}

inline void OnFrameChange(ActiveContext& context) {
	// Free the previous model
	context.renderContext.model->free();

	// Update the active model
	context.renderContext.model = MolecularModelFromAtoms((*context.frames).atoms[context.activeFrame], &context.renderContext.lightingShader, context.style);
	ResetViewSelection(context);
	context.permanentSelection.clear();
}

inline void OnAnimationModeStart(ActiveContext& context) {
	context.mode = ANIMATION;
}

void CheckForAndHandleFrameChange(ActiveContext& context) {
	// Handle switching active frame on key press
	if (IsKeyPressed(KEY_RIGHT)) {
		if ((context.activeFrame + 1) < context.numFrames) {
			context.activeFrame += 1;
			OnFrameChange(context);
		}
	}
	else if (IsKeyPressed(KEY_LEFT)) {
		if (context.activeFrame > 0) {
			context.activeFrame -= 1;
			OnFrameChange(context);
		}
	}
}

void CheckForAndHandleModeChange(ActiveContext& context) {
	// We allow mode change either by pressing "M" or via the dropdown box
	int mode = static_cast<int>(context.mode);
	GuiSetStyle(DROPDOWNBOX, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
    if (GuiDropdownBox((Rectangle){ 10, 10, 125, 30 }, "VIEW;EDIT;ANIMATION", &mode, context.modeDropdownEdit)) {
    	if (context.modeDropdownEdit)
    		context.mode = static_cast<InteractionMode>(mode);
    	context.modeDropdownEdit = !context.modeDropdownEdit;
	} else if (IsKeyPressed(KEY_M)) {
		context.mode == static_cast<InteractionMode>(static_cast<int>(NUMBER_OF_MODES) - 1) ? context.mode = static_cast<InteractionMode>(0) : context.mode = static_cast<InteractionMode>(static_cast<int>(context.mode) + 1);
	}
}