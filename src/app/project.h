#pragma once
// A ChemLab project is a folder on disk with a `chemlab.toml` at its root.
// Everything the project references (structures, the saved dock layout,
// later analysis scripts) is stored relative to that folder, so a project
// can be moved, zipped or version-controlled as a unit.
//
//   my_project/
//     chemlab.toml      <- ProjectConfig (this file)
//     layout.ini        <- ImGui dock layout, written by ChemLab
//     data/traj.xyz     <- referenced from chemlab.toml as "data/traj.xyz"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct ProjectStructureEntry {
    std::string path;                 // relative to the project root when possible
    std::string name;                 // display name (defaults to the file name)
    int frame = 0;                    // 0-based frame to show
    std::vector<std::vector<int>> measurements;   // 1-based atom indices, 2..4 per entry
};

struct ProjectViewSettings {
    std::string style = "ball-and-stick";
    std::string background = "#1e1e1e";
    bool grid = true;
    bool atomNumbers = false;
    float ballScale = 0.25f;
    float stickRadius = 0.2f;
    float sphereScale = 1.0f;
    int twoDPlotIndex = 0;
    float bondTolerance = 0.4f;
};

struct ProjectConfig {
    std::string name;
    std::string description;
    std::string layoutFile = "layout.ini";        // relative to root
    ProjectViewSettings view;
    std::vector<ProjectStructureEntry> structures;
    int activeStructure = 0;
    std::vector<std::string> startupCommands;     // run after everything is loaded
    std::vector<std::string> scripts;             // reserved: analysis scripts, relative paths
};

class Project {
public:
    static constexpr const char* kConfigFileName = "chemlab.toml";

    // `path` may be the project directory or the toml file itself.
    static std::optional<Project> Load(const std::filesystem::path& path, std::string& error);
    // Creates <dir>/chemlab.toml (and the directory) with an empty config.
    static std::optional<Project> Create(const std::filesystem::path& dir, const std::string& name, std::string& error);
    // True if `path` is a project directory or a chemlab.toml file.
    static bool LooksLikeProject(const std::filesystem::path& path);

    bool Save(std::string& error) const;

    const std::filesystem::path& Root() const { return root; }
    std::filesystem::path ConfigPath() const { return root / kConfigFileName; }
    std::filesystem::path LayoutPath() const { return Resolve(config.layoutFile); }

    // Absolute path for a project-relative (or already absolute) path.
    std::filesystem::path Resolve(const std::string& relativeOrAbsolute) const;
    // Project-relative form of an absolute path when it lies under the root,
    // otherwise the absolute path unchanged.
    std::string Relativise(const std::filesystem::path& absolute) const;

    ProjectConfig config;

private:
    std::filesystem::path root;
};

// Serialisation (exposed for tests / tooling).
std::string SerialiseProjectConfig(const ProjectConfig& config);
bool ParseProjectConfig(const std::string& toml, ProjectConfig& out, std::string& error);
