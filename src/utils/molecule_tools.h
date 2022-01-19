#pragma once

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
