#pragma once

BallAndStickModel* BallAndStickModelFromAtoms(const Atoms& atoms, Shader* shader) {
	BallAndStickModel* model = new BallAndStickModel;

	// There are only two meshes needed for the ball and stick model,
    // so we generate those and store the appropriate transforms and materials
    // for each atom.
	model->sphereMesh = (GenMeshSphere(1.0f, 16, 16));
	model->stickMesh  = (GenMeshCylinder(g_stickRadius, 1.0f, 12));
	model->meshes.push_back(GenMeshSphere(1.0f, 16, 16)); // sphere mesh
	model->meshes.push_back(GenMeshCylinder(g_stickRadius, 1.0f, 12)); // stick mesh
	model->numSpheres = atoms.natoms;
	model->numSticks  = 2 * atoms.covalentBondList.pairs.size();
	model->stickIndices.resize(atoms.natoms); // ensure we have a stick index for every atom

	model->modelTransform = MatrixIdentity();
	// Get the sphere transforms and materials
	const int sphereMeshIndex = 0;
	for (int i = 0; i < atoms.natoms; i++) {
		if (model->labelToMaterialIndex.count(atoms.labels[i])) {
			// Create a new entry in map from index of an atom to corresponding rendering indices.
			model->IDToEntityIndices[i] = {
				sphereMeshIndex,
				model->labelToMaterialIndex[atoms.labels[i]],
				(int)model->transforms[model->labelToMaterialIndex[atoms.labels[i]]].size()
			};
			// Store transform of this entity in the array for its corresponding material.
			model->transforms[model->labelToMaterialIndex[atoms.labels[i]]].push_back(MatrixScale(atoms.renderData[i].vdwRadius * g_ballScale) * MatrixTranslate(atoms.xyz[i]));
		} else {
			// CREATE A NEW MATERIAL //
			Material material = LoadMaterialDefault();
			material.shader = *shader;
    		material.maps[MATERIAL_MAP_DIFFUSE].color = atoms.renderData[i].color;
    		model->labelToMaterialIndex[atoms.labels[i]] = model->materials.size(); // make new material index for this atom type
    		model->meshIndexToRenderingIndices[sphereMeshIndex].push_back({
    			(int)model->materials.size(), // material index for this material
    			0, // transforms always start at index 0 for spheres
    			(int)std::count(atoms.labels.begin(), atoms.labels.end(), atoms.labels[i]) // number of atoms of this type in the model
    		});
    		model->materials.push_back(material); // store the new material
			model->transforms.push_back({MatrixScale(atoms.renderData[i].vdwRadius * g_ballScale) * MatrixTranslate(atoms.xyz[i])});
			// ^^^ Push back a new VECTOR containing the transform for the first atom having the new material.
			// Store entity indices as well.
			model->IDToEntityIndices[i] = {
				sphereMeshIndex,
				model->labelToMaterialIndex[atoms.labels[i]],
				0
			};
		}
	}

	// store the number of bonds to each atom type so we can get renderingIndices for all sticks.
	std::map<std::string, int> numBondsToLabel;
	const int stickMeshIndex = 1;
	for (int i = 0; i < atoms.covalentBondList.pairs.size(); i++) {
		const int idx_1 = atoms.covalentBondList.pairs[i].first;
		const int idx_2 = atoms.covalentBondList.pairs[i].second;
		const std::string atom_label_1 = atoms.labels[idx_1];
		const std::string atom_label_2 = atoms.labels[idx_2];

		// If we've seen a bond to this atom type then increment count by one
		// otherwise start at one. This is used to construct the rendering indices
		// which give us the range of indices over which the sticks should be rendered
		// for a given material.
		if (numBondsToLabel.count(atom_label_1))
			numBondsToLabel[atom_label_1] += 1;
		else {
			numBondsToLabel[atom_label_1] = 1;
		}
		if (numBondsToLabel.count(atom_label_2))
			numBondsToLabel[atom_label_2] += 1;
		else {
			numBondsToLabel[atom_label_2] = 1;
		}

		Vector3 bondVector = atoms.xyz[idx_2] - atoms.xyz[idx_1];
		Vector3 bondOrigin = atoms.xyz[idx_1];
		Vector3 bondMiddle = atoms.xyz[idx_1] + 0.5 * bondVector;
		
		Matrix firstStickTransform  = MatrixScale((Vector3){1.0f, 0.5f * norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondOrigin);
		Matrix secondStickTransform = MatrixScale((Vector3){1.0f, 0.5f * norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondMiddle);
		
		// Find material of atom to which each stick belongs //
		// FIRST STICK //
		int firstAtomMaterialIndex = model->labelToMaterialIndex[atom_label_1];
		// Create a new entry in map from index of an atom to corresponding rendering indices.
		const int stick_index_1 = (int)model->transforms[firstAtomMaterialIndex].size();
		model->IDToEntityIndices[atoms.natoms + 2 * i] = {
			stickMeshIndex,
			firstAtomMaterialIndex,
			stick_index_1
		};
		model->stickMaterialAndTransformToID[{firstAtomMaterialIndex, stick_index_1}] = atoms.natoms + 2 * i;
		// Store transform of this entity in the array for its corresponding material. Stored after the atom which has same material.
		model->transforms[model->labelToMaterialIndex[atom_label_1]].push_back(firstStickTransform);
		
		// SECOND STICK //
		int secondAtomMaterialIndex = model->labelToMaterialIndex[atom_label_2];
		// Create a new entry in map from index of an atom to corresponding rendering indices.
		const int stick_index_2 = (int)model->transforms[secondAtomMaterialIndex].size();
		model->IDToEntityIndices[atoms.natoms + 2 * i + 1] = {
			stickMeshIndex,
			secondAtomMaterialIndex,
			stick_index_2
		};
		model->stickMaterialAndTransformToID[{secondAtomMaterialIndex, stick_index_2}] = atoms.natoms + 2 * i + 1;
		// Store transform of this entity in the array for its corresponding material. Stored after the atom which has same material.
		model->transforms[model->labelToMaterialIndex[atom_label_2]].push_back(secondStickTransform);

		// store the transform indices of each half-bond associated with each atom, for updating materials and transforms as needed
		// These are indices into the transform array for the material of this atom.
		model->stickIndices[idx_1].push_back(stick_index_1);
		model->stickIndices[idx_2].push_back(stick_index_2);

	}
	// CREATE RENDERING INDICES ENTRIES FOR EACH STICK //
	for (const auto& pair : numBondsToLabel) {
		const int finalIndexOfSpheres = (int)std::count(atoms.labels.begin(), atoms.labels.end(), pair.first); // number of atoms of this type in the model
		model->meshIndexToRenderingIndices[stickMeshIndex].push_back({
    		(int)model->labelToMaterialIndex[pair.first],
    		finalIndexOfSpheres,
    		numBondsToLabel[pair.first]
    	});
	}
	return model;
}

/*SticksModel* SticksModelFromAtoms(const Atoms& atoms, Shader* shader) {
	SticksModel* model = new SticksModel;

	// There are only two meshes needed for the ball and stick model,
    // so we generate those and store the appropriate transforms and materials
    // for each atom.
	model->stickMesh  = (GenMeshCylinder(g_stickRadius, 1.0f, 64));
	model->numSticks  = atoms.covalentBondList.pairs.size();

	model->transforms.reserve(model->numSticks);
	
	// Get the cylinder transforms and materials
	for (int i = 0; i < atoms.covalentBondList.pairs.size(); i++) {
		Vector3 bondVector = atoms.xyz[idx_2] - atoms.xyz[idx_1];

		Vector3 bondOrigin = atoms.xyz[idx_1];
		model->transforms.push_back( MatrixScale((Vector3){1.0f, norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondOrigin));

		Material material = LoadMaterialDefault();
		material.shader = *shader;
    	material.maps[MATERIAL_MAP_DIFFUSE].color = atoms.renderData[i].color;
    	model->materials.push_back(material);
	}
	return model;
}

SpheresModel* SpheresModelFromAtoms(const Atoms& atoms, Shader* shader) {
	SpheresModel* model = new SpheresModel;

	model->sphereMesh = (GenMeshSphere(1.0f, 32, 32));
	model->numSpheres = atoms.natoms;

	model->transforms.reserve(model->numSpheres);
	// Get the sphere transforms and materials
	for (int i = 0; i < atoms.natoms; i++) {
		model->transforms.push_back(MatrixScale(atoms.renderData[i].vdwRadius) * MatrixTranslate(atoms.xyz[i]));

		Material material = LoadMaterialDefault();
		material.shader = *shader;
    	material.maps[MATERIAL_MAP_DIFFUSE].color = atoms.renderData[i].color;
    	model->materials.push_back(material);
	}

	return model;
}*/

MolecularModel* MolecularModelFromAtoms(const Atoms& atoms, Shader* shader, const RenderStyle style) {
	switch(style) {
		case BALL_AND_STICK:
			return BallAndStickModelFromAtoms(atoms, shader);
			break;
		//case SPHERES:
		//	return SpheresModelFromAtoms(atoms, shader);
		//	break;
		//case STICKS:
		//	return SticksModelFromAtoms(atoms, shader);
		//	break;
		default :
			return BallAndStickModelFromAtoms(atoms, shader);
	}
}

RenderData GetRenderData(const std::string& atomLabel) {
	if (atomColors.count(atomLabel) && atomVdwRadii.count(atomLabel))
		return (RenderData){atomColors[atomLabel], atomVdwRadii[atomLabel], covalentRadii[atomLabel]};
	else // There should be default element data we use for this situation.
		throw std::invalid_argument("Not a valid element.");
}

Frames readXYZ(const std::string& file)
{
	if (!std::filesystem::exists(file))
	{
		throw std::filesystem::filesystem_error("Could not find geomtry file (.xyz)\n", std::error_code());
	}
	
	// Everything that goes in the Frames object
	uint32_t nframes = 0;
	std::vector<std::string> headers;
	std::vector<Atoms> frames;
	std::unordered_map<std::string, std::filesystem::file_time_type> loadedFiles;

	//open the file and store time of last modification
	const std::filesystem::path filePath = file;
	loadedFiles[filePath.string()] = std::filesystem::last_write_time(filePath);
	std::ifstream infile(file);

	std::string line;
	// Outer while loop until end of file is reached
	while (std::getline(infile, line)) {
		if (std::all_of(line.begin(), line.end(), isspace)) {
			continue; // skip lines with only white space between frames
		} else { // Parse an entire frame

			nframes += 1;
			// store the number of atoms and comment line, then parse by tokens
			uint32_t natoms = std::stoi(line);

			std::getline(infile, line);
			headers.push_back(line);

			//read all of the coordinates and atom labels and store in an Atoms object
			Atoms atoms;
			atoms.natoms = natoms;
			std::vector<Vector3> coordinates;
			coordinates.reserve(natoms);
			for (int i = 0; i < natoms; ++i)
			{
				std::getline(infile, line);
				std::istringstream iss(line);
				std::string atomLabel;
				float x, y, z;
				if (!(iss >> atomLabel >> x >> y >> z))
					throw "File does not have appropriate atomLabel x y z format.";

				atoms.labels.push_back(atomLabel);
				atoms.renderData.push_back(GetRenderData(atomLabel));
				coordinates.push_back((Vector3){x, y, z});
			}
			atoms.xyz = coordinates;
			atoms.covalentBondList = MakeCovalentBondList(atoms);
			frames.push_back(atoms);
		}
	}
	return Frames(nframes, frames, headers, loadedFiles);
}

std::string GetXYZFormattedString(const Atoms& atoms) {
	std::string xyzString = fmt::format("{}\n\n", atoms.xyz.size());
	for (int i = 0; i < atoms.xyz.size(); ++i) {
		xyzString += fmt::format("{}  {} {} {}\n", atoms.labels[i], atoms.xyz[i].x, atoms.xyz[i].y, atoms.xyz[i].z);
	}
	return xyzString;
}