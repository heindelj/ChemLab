#pragma once

void OnAtomMove(Atoms& atoms, std::vector<int> updatedIndices, MolecularModel& model) {
	// Recalculate bonds and update transforms appropriately
	// SPEED: only need to recalculate for atoms that moved!
	atoms.covalentBondList = MakeCovalentBondList(atoms);

	// Update model transforms
	for (auto it = updatedIndices.begin(); it != updatedIndices.end(); ++it)
		UpdateTransformPosition(model.transforms[*it], atoms.xyz[*it]);
}

void OnAtomMove(Atoms& atoms, int index, MolecularModel& model) {
	// Recalculate bonds and update transforms appropriately
	// SPEED: only need to recalculate for atoms that moved!
	atoms.covalentBondList = MakeCovalentBondList(atoms);

	// Update model transforms
	UpdateTransformPosition(model.transforms[index], atoms.xyz[index]);
}

void ResetViewSelection(ActiveContext& context) {
	context.selectionStep = NONE;
	context.viewSelection.fill(-1);
}

void OnFrameChange(ActiveContext& context) {
	// Free the previous model
	context.renderContext.model->free();

	// Update the active model
	context.renderContext.model = MolecularModelFromAtoms((*context.frames).atoms[context.activeFrame], &context.renderContext.lightingShader, context.style);
	ResetViewSelection(context);
	context.permanentSelection.clear();
	context.atomsToHighlight.clear();
}

inline void OnAnimationModeStart(ActiveContext& context) {
	context.mode = ANIMATION;
}

void CheckForAndHandleFrameChange(ActiveContext& context) {
	// Handle switching active frame on key press
	if (IsKeyPressed(KEY_RIGHT)) {
		if ((context.activeFrame + 1) < context.frames->nframes) {
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