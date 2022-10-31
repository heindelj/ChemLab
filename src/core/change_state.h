#pragma once

void UpdateMaterials(const Color& color, const std::set<int>& updatedIndices, MolecularModel& model) {
	// TODO: This function will occasonally destroy everything because I don't actually remove the old data.
	// Instead I just render over it which can mess up the old transforms or something resulting in a disaster.

	// CREATE A NEW MATERIAL //
	Material material = LoadMaterialDefault();
	material.shader = model.materials[0].shader; // Every material uses same shader so just grab from index 0
	material.maps[MATERIAL_MAP_DIFFUSE].color = color;
	model.meshIndexToRenderingIndices[0].push_back({
		(int)model.materials.size(), // material index for this material
		0, // transforms always start at index 0 for spheres
		(int)updatedIndices.size() // number of atoms which will have this new material
	});
	const int newMaterialIndex = (int)model.materials.size();
	model.materials.push_back(material); // store the new material

	const int oldMaterialIndex = model.IDToEntityIndices[*updatedIndices.begin()].materialIndex; // need this for sticks later

	// Rearrange all the data that needs to be moved when updating a material
	int loopCounter = 0;
	for (auto it = updatedIndices.begin(); it != updatedIndices.end(); ++it) {
		EntityIndices indices = model.IDToEntityIndices[*it];

		// move the transforms from old slot to new slot
		if (loopCounter == 0) {
			model.transforms.push_back({model.transforms[indices.materialIndex][indices.transformIndex]});
		} else {
			model.transforms[newMaterialIndex].push_back(model.transforms[indices.materialIndex][indices.transformIndex]);
		}
		// Store entity indices as well.
		model.IDToEntityIndices[*it] = {
			indices.meshIndex,
			newMaterialIndex,
			loopCounter
		};
		loopCounter++;
	}

	// Update the stick data for each atom
	int numSticks = 0;
	for (auto it = updatedIndices.begin(); it != updatedIndices.end(); ++it) {
		EntityIndices indices = model.IDToEntityIndices[*it];
		for (int i_stick = 0; i_stick < model.stickIndices[*it].size(); ++i_stick) {
			// move the transforms from old slot to new slot
			model.transforms[newMaterialIndex].push_back(model.transforms[oldMaterialIndex][model.stickIndices[*it][i_stick]]);
			// Store entity indices as well.
			const int stickID = model.stickMaterialAndTransformToID[{oldMaterialIndex, model.stickIndices[*it][i_stick]}];
			model.IDToEntityIndices[stickID] = {
				1,
				newMaterialIndex,
				(int)(model.transforms[newMaterialIndex].size() - 1)
			};
			numSticks++;
		}
	}
	model.meshIndexToRenderingIndices[1].push_back({
		newMaterialIndex,
		(int)updatedIndices.size(),
		numSticks
	});
}

void OnAtomMove(Atoms& atoms, const std::vector<int>& updatedIndices, MolecularModel& model) {
	// Recalculate bonds and update transforms appropriately
	// SPEED: only need to recalculate for atoms that moved!
	atoms.covalentBondList = MakeCovalentBondList(atoms);

	// Update model transforms
	for (auto it = updatedIndices.begin(); it != updatedIndices.end(); ++it) {
		EntityIndices indices = model.IDToEntityIndices[*it];
		UpdateTransformPosition(model.transforms[indices.materialIndex][indices.transformIndex], atoms.xyz[*it]);
	}
}

void OnAtomMove(Atoms& atoms, const std::set<int>& updatedIndices, MolecularModel& model) {
	// Recalculate bonds and update transforms appropriately
	// SPEED: only need to recalculate for atoms that moved!
	atoms.covalentBondList = MakeCovalentBondList(atoms);
	// TODO: Need to take the cylinder transform calcualtion and do it here after
	// updating the covalent bond list so that bonds also update when atoms move.

	// Update model transforms
	for (auto it = updatedIndices.begin(); it != updatedIndices.end(); ++it) {
		EntityIndices indices = model.IDToEntityIndices[*it];
		UpdateTransformPosition(model.transforms[indices.materialIndex][indices.transformIndex], atoms.xyz[*it]);
	}

	/*for (int i : updatedIndices) {
		//std::cout << i << " " << atoms.covalentBondList.pairs[i].first << " " << atoms.covalentBondList.pairs[i].second << std::endl;
		Vector3 bondVector = atoms.xyz[atoms.covalentBondList.pairs[i].second] - atoms.xyz[atoms.covalentBondList.pairs[i].first];

		Vector3 bondOrigin = atoms.xyz[atoms.covalentBondList.pairs[i].first];
		Vector3 bondMiddle = atoms.xyz[atoms.covalentBondList.pairs[i].first] + 0.5 * bondVector;
		model.transforms.push_back( MatrixScale((Vector3){1.0f, 0.5f * norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondOrigin));
		model.transforms.push_back( MatrixScale((Vector3){1.0f, 0.5f * norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondMiddle));

		// store the indices of each half-bond associated with each atom, for updating materials and transforms as needed
		model.stickIndices[atoms.covalentBondList.pairs[i].first ].push_back(model.transforms.size() - 2);
		model.stickIndices[atoms.covalentBondList.pairs[i].second].push_back(model.transforms.size() - 1);
	}*/
}

void OnAtomMove(Atoms& atoms, int index, MolecularModel& model) {
	// Recalculate bonds and update transforms appropriately
	// SPEED: only need to recalculate for atoms that moved!
	atoms.covalentBondList = MakeCovalentBondList(atoms);

	// Update model transforms
	EntityIndices indices = model.IDToEntityIndices[index];
	UpdateTransformPosition(model.transforms[indices.materialIndex][indices.transformIndex], atoms.xyz[index]);
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
	//if (!context.lockCamera)
	//	context.renderContext.camera = GetCameraWithGoodDefaultPosition(context.frames->atoms[context.activeFrame].xyz);
	UpdateLighting(context.renderContext);

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