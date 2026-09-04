#pragma once
// Trajectories (Frames of ChemicalData), geometry helpers and xyz-header
// energy parsing.
// Nothing in here knows about rendering or the UI.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "raylib.h"

#include "core/chemical_data.h"

// A trajectory: one ChemicalData per frame plus per-frame metadata.
struct Frames {
    uint32_t nframes = 0;
    std::vector<ChemicalData> data;
    std::vector<std::string> headers;
    // Energy parsed from each comment line (NaN when absent), cached at load
    // time: parsing 15k headers per rendered frame is what killed large files.
    std::vector<double> energies;
    bool anyEnergy = false;
    // Bumped whenever the coordinate data is replaced (hot reload), so plot
    // caches know to rebuild.
    uint64_t dataVersion = 0;
    // File paths and last modification time, used to hot-reload edited files.
    std::unordered_map<std::string, std::filesystem::file_time_type> loadedFiles;
};

// Position of atom i as a raylib vector (R is stored as doubles).
inline Vector3 AtomPos(const ChemicalData& c, size_t i) {
    return Vector3{(float)c.R[3 * i], (float)c.R[3 * i + 1], (float)c.R[3 * i + 2]};
}

// Geometry helpers
float Distance(const Vector3& a, const Vector3& b);
float AngleDeg(const Vector3& a, const Vector3& b, const Vector3& c);       // angle at b
float DihedralDeg(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d);
Vector3 Centroid(const std::vector<Vector3>& points);

// Try to pull an energy-like number out of an xyz comment line. Understands
// extxyz key=value headers ("energy=-76.4", "energy_hartree=-153.2", keys
// containing "energy" except bias/confine terms), "E = -76.4" / "energy: x"
// styles, and finally a bare leading float -- but only when the line is not
// key=value formatted, so it never grabs some unrelated field's number.
// Hand-rolled (no std::regex): it runs over every frame of a file at load.
std::optional<double> ParseEnergyFromHeader(const std::string& header);
// Fills frames.energies/anyEnergy from frames.headers.
void CacheFrameEnergies(Frames& frames);
