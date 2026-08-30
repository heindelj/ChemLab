#pragma once
#include <string>

#include "core/molecule.h"

// Reads a (multi-frame) xyz file. Throws std::runtime_error on a parse
// failure and std::filesystem::filesystem_error if the file is missing.
Frames ReadXYZ(const std::string& path);

// Formats a single frame as xyz text.
std::string FormatXYZ(const Atoms& atoms, const std::string& header = "");

// Writes one frame (frameIndex >= 0) or every frame (frameIndex < 0).
// Returns false on failure.
bool WriteXYZ(const std::string& path, const Frames& frames, int frameIndex);

// Re-reads any loaded file whose modification time changed. Returns true if
// the frames were replaced.
bool CheckForFileChangesAndUpdate(Frames& frames);
