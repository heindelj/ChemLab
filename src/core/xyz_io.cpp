#include "core/xyz_io.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <fmt/format.h>

Frames ReadXYZ(const std::string& file) {
    if (!std::filesystem::exists(file))
        throw std::filesystem::filesystem_error("Could not find geometry file (.xyz)", file, std::error_code());

    Frames frames;
    const std::filesystem::path filePath = file;
    frames.loadedFiles[filePath.string()] = std::filesystem::last_write_time(filePath);

    std::ifstream infile(file);
    std::string line;
    int lineNumber = 0;
    while (std::getline(infile, line)) {
        lineNumber++;
        if (std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isspace(c); }))
            continue;  // blank lines between frames

        uint32_t natoms = 0;
        try {
            natoms = static_cast<uint32_t>(std::stoul(line));
        } catch (...) {
            throw std::runtime_error(fmt::format("{}:{}: expected an atom count, got '{}'", file, lineNumber, line));
        }

        std::getline(infile, line);
        lineNumber++;
        frames.headers.push_back(line);

        Atoms atoms;
        atoms.natoms = natoms;
        atoms.xyz.reserve(natoms);
        for (uint32_t i = 0; i < natoms; ++i) {
            if (!std::getline(infile, line))
                throw std::runtime_error(fmt::format("{}: unexpected end of file inside frame {}", file, frames.nframes + 1));
            lineNumber++;
            std::istringstream iss(line);
            std::string atomLabel;
            float x, y, z;
            if (!(iss >> atomLabel >> x >> y >> z))
                throw std::runtime_error(fmt::format("{}:{}: expected 'label x y z', got '{}'", file, lineNumber, line));
            if (!IsKnownElement(atomLabel))
                throw std::runtime_error(fmt::format("{}:{}: unknown element '{}'", file, lineNumber, atomLabel));
            atoms.labels.push_back(atomLabel);
            atoms.renderData.push_back(GetRenderData(atomLabel));
            atoms.xyz.push_back(Vector3{x, y, z});
        }
        atoms.covalentBondList = MakeCovalentBondList(atoms);
        frames.atoms.push_back(std::move(atoms));
        frames.nframes++;
    }
    if (frames.nframes == 0)
        throw std::runtime_error(fmt::format("{}: no frames found", file));
    CacheFrameEnergies(frames);
    return frames;
}

std::string FormatXYZ(const Atoms& atoms, const std::string& header) {
    std::string out = fmt::format("{}\n{}\n", atoms.natoms, header);
    for (uint32_t i = 0; i < atoms.natoms; ++i)
        out += fmt::format("{:<3} {:14.8f} {:14.8f} {:14.8f}\n", atoms.labels[i], atoms.xyz[i].x, atoms.xyz[i].y, atoms.xyz[i].z);
    return out;
}

bool WriteXYZ(const std::string& path, const Frames& frames, int frameIndex) {
    std::ofstream out(path);
    if (!out) return false;
    if (frameIndex >= 0) {
        if (frameIndex >= (int)frames.nframes) return false;
        out << FormatXYZ(frames.atoms[frameIndex], frames.headers[frameIndex]);
    } else {
        for (uint32_t i = 0; i < frames.nframes; ++i)
            out << FormatXYZ(frames.atoms[i], frames.headers[i]);
    }
    return static_cast<bool>(out);
}

bool CheckForFileChangesAndUpdate(Frames& frames) {
    bool didUpdate = false;
    for (auto& [currentFile, storedWriteTime] : frames.loadedFiles) {
        try {
            const std::filesystem::path file = currentFile;
            const auto lastWriteTime = std::filesystem::last_write_time(file);
            if (storedWriteTime != lastWriteTime) {
                // Re-read from scratch. Good enough for the "edit a file in an
                // editor and watch it update" workflow.
                Frames newFrames = ReadXYZ(file.string());
                frames.nframes = newFrames.nframes;
                frames.atoms = std::move(newFrames.atoms);
                frames.headers = std::move(newFrames.headers);
                frames.energies = std::move(newFrames.energies);
                frames.anyEnergy = newFrames.anyEnergy;
                frames.dataVersion++;
                storedWriteTime = lastWriteTime;
                didUpdate = true;
            }
        } catch (...) {
            // The file is mid-write (editors often replace it via a temp file)
            // or was deleted. Try again next frame.
            return false;
        }
    }
    return didUpdate;
}
