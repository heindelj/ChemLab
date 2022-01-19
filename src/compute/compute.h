#pragma once
/////////////////////////////////////////////////////////////////
// Currently this isn't used for anything, but this will
// be where we add all of the conversion to eigen and implement
// force fields, optimizations, basin hopping, etc.
// Everything compute intensive takes place over here while
// everything rendering related happens in a separate module
// working with different representations of the data.
/////////////////////////////////////////////////////////////////


#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>

#include <Eigen/Dense>

typedef Eigen::Matrix<double, Eigen::Dynamic, 3> MatrixX3d;
struct Atoms {
	uint32_t natoms;
	MatrixX3d xyz;
	std::vector<std::string> labels;
};

struct Frames {
	uint32_t nframes;
	std::vector<Atoms> atoms;
	std::vector<std::string> headers;
};

#include "debug.h"

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
			MatrixX3d coordinates;
			coordinates.resize(natoms, 3);
			for (int i = 0; i < natoms; ++i)
			{
				std::getline(infile, line);
				std::istringstream iss(line);
				std::string atom_label;
				double x, y, z;
				if (!(iss >> atom_label >> x >> y >> z))
					throw "File does not have appropriate atom_label x y z format.";

				atoms.labels.push_back(atom_label);
				coordinates(i, 0) = x;
				coordinates(i, 1) = y;
				coordinates(i, 2) = z;
			}
			atoms.xyz = coordinates;
			frames.push_back(atoms);
		}
	}
	return Frames(nframes, frames, headers);
}
