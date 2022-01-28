#pragma once


BallAndStickModel* BallAndStickModelFromAtoms(const Atoms& atoms, Shader* shader) {
	BallAndStickModel* model = new BallAndStickModel;

	// There are only two meshes needed for the ball and stick model,
    // so we generate those and store the appropriate transforms and materials
    // for each atom.
	model->sphereMesh = (GenMeshSphere(1.0f, 24, 24));
	model->stickMesh  = (GenMeshCylinder(g_stickRadius, 1.0f, 64));
	model->numSpheres = atoms.natoms;
	model->numSticks  = 2 * atoms.covalentBondList.pairs.size();

	model->transforms.reserve(model->numSpheres + model->numSticks);
	// Get the sphere transforms and materials
	for (int i = 0; i < atoms.natoms; i++) {
		model->transforms.push_back(MatrixScale(atoms.renderData[i].vdwRadius * g_ballScale) * MatrixTranslate(atoms.xyz[i]));

		Material material = LoadMaterialDefault();
		material.shader = *shader;
    	material.maps[MATERIAL_MAP_DIFFUSE].color = atoms.renderData[i].color;
    	model->materials.push_back(material);
	}

	// Get the cylinder transforms and materials
	for (int i = 0; i < atoms.covalentBondList.pairs.size(); i++) {
		Vector3 bondVector = atoms.xyz[atoms.covalentBondList.pairs[i].second] - atoms.xyz[atoms.covalentBondList.pairs[i].first];

		Vector3 bondOrigin = atoms.xyz[atoms.covalentBondList.pairs[i].first];
		Vector3 bondMiddle = atoms.xyz[atoms.covalentBondList.pairs[i].first] + 0.5 * bondVector;
		model->transforms.push_back( MatrixScale((Vector3){1.0f, 0.5f * norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondOrigin));
		model->transforms.push_back( MatrixScale((Vector3){1.0f, 0.5f * norm(bondVector), 1.0f}) * MatrixAlignToAxis((Vector3){0.0f, 1.0f, 0.0f}, bondVector) * MatrixTranslate(bondMiddle));

		// first stick
		Material material = LoadMaterialDefault();
		material.shader = *shader;
    	material.maps[MATERIAL_MAP_DIFFUSE].color = atoms.renderData[atoms.covalentBondList.pairs[i].first].color;
    	model->materials.push_back(material);

    	// second stick
    	material = LoadMaterialDefault();
    	material.shader = *shader;
    	material.maps[MATERIAL_MAP_DIFFUSE].color = atoms.renderData[atoms.covalentBondList.pairs[i].second].color;
    	model->materials.push_back(material);
	}
	return model;
}

SticksModel* SticksModelFromAtoms(const Atoms& atoms, Shader* shader) {
	SticksModel* model = new SticksModel;

	// There are only two meshes needed for the ball and stick model,
    // so we generate those and store the appropriate transforms and materials
    // for each atom.
	model->stickMesh  = (GenMeshCylinder(g_stickRadius, 1.0f, 64));
	model->numSticks  = atoms.covalentBondList.pairs.size();

	model->transforms.reserve(model->numSticks);
	
	// Get the cylinder transforms and materials
	for (int i = 0; i < atoms.covalentBondList.pairs.size(); i++) {
		Vector3 bondVector = atoms.xyz[atoms.covalentBondList.pairs[i].second] - atoms.xyz[atoms.covalentBondList.pairs[i].first];

		Vector3 bondOrigin = atoms.xyz[atoms.covalentBondList.pairs[i].first];
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
}

MolecularModel* MolecularModelFromAtoms(const Atoms& atoms, Shader* shader, const RenderStyle style) {
	switch(style) {
		case BALL_AND_STICK:
			return BallAndStickModelFromAtoms(atoms, shader);
			break;
		case SPHERES:
			return SpheresModelFromAtoms(atoms, shader);
			break;
		case STICKS:
			return SticksModelFromAtoms(atoms, shader);
			break;
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
		throw "Could not find geometry file (.xyz)!";
	}
	
	// Everything that goes in the Frames object
	uint32_t nframes = 0;
	std::vector<std::string> headers;
	std::vector<Atoms> frames;

	//open the file
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
			atoms.covalentBondList = makeCovalentBondList(atoms);
			frames.push_back(atoms);
		}
	}
	return Frames(nframes, frames, headers);
}
