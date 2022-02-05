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