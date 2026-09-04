#pragma once
// A ChemLab project is a folder on disk with a `chemlab.toml` at its root.
// Everything the project references (structures, scenes, graphs, scripts, the
// saved dock layout) is stored relative to that folder, so a project can be
// moved, zipped or version-controlled as a unit. While a project is open its
// root is the process working directory, so bare file names everywhere
// (`load`, `screenshot`, script nodes, file dialogs) resolve inside it.
//
//   my_project/
//     chemlab.toml      <- ProjectConfig (this file)
//     layout.ini        <- ImGui dock layout, written by ChemLab
//     data/traj.xyz     <- referenced from chemlab.toml as "data/traj.xyz"
//     scenes/*.json     <- scene graphs (layouts)
//     graphs/*.json     <- named node graphs from the Graph Canvas
//     scripts/*.py      <- python node scripts
//     output/           <- screenshots and exports

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ProjectStructureEntry {
    std::string path;                 // relative to the project root when possible
    std::string name;                 // display name (defaults to the file name)
    int frame = 0;                    // 0-based frame to show
    std::vector<std::vector<int>> measurements;   // 1-based atom indices, 2..4 per entry
};

// [paths]: where the project keeps things, relative to the root.
struct ProjectPaths {
    std::vector<std::string> data = {"data"};   // searched in order for bare structure names
    std::string scenes = "scenes";
    std::string graphs = "graphs";
    std::string workflows = "workflows";
    std::string scripts = "scripts";
    std::string output = "output";
    std::string layout = "layout.ini";          // ImGui dock state
};

// [scene]: what the screen shows when the project opens (replaces the global
// chemlab_scene.toml for projects).
struct ProjectSceneSettings {
    std::string active = "classic";   // scene name: built-in or scenes/<name>.json
    std::string layout;               // layout within the scene ("" = the scene's first)
};

// [python]: interpreter for script nodes; a relative path is under the root.
struct ProjectPython {
    std::string interpreter = "python3";
    std::vector<std::pair<std::string, std::string>> env;   // extra environment for scripts
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
    static constexpr int kFormat = 1;             // bump when the schema changes incompatibly

    std::string name;
    std::string description;
    int format = kFormat;
    ProjectPaths paths;
    ProjectSceneSettings scene;
    ProjectViewSettings view;
    ProjectPython python;
    std::vector<ProjectStructureEntry> structures;
    int activeStructure = 0;
    std::vector<std::string> startupCommands;     // run after everything is loaded
    std::vector<std::string> scripts;             // project scripts, relative to paths.scripts
};

class Project {
public:
    static constexpr const char* kConfigFileName = "chemlab.toml";

    // `path` may be the project directory or the toml file itself.
    static std::optional<Project> Load(const std::filesystem::path& path, std::string& error);
    // Creates <dir>/chemlab.toml (and the directory + the default folders)
    // with a default config.
    static std::optional<Project> Create(const std::filesystem::path& dir, const std::string& name, std::string& error);
    // True if `path` is a project directory or a chemlab.toml file.
    static bool LooksLikeProject(const std::filesystem::path& path);

    bool Save(std::string& error) const;
    // Create the folders named in [paths] if they are missing (best effort).
    void EnsureFolders() const;

    const std::filesystem::path& Root() const { return root; }
    std::filesystem::path ConfigPath() const { return root / kConfigFileName; }
    std::filesystem::path LayoutPath() const { return Resolve(config.paths.layout); }
    std::filesystem::path ScenesDir() const { return Resolve(config.paths.scenes); }
    std::filesystem::path GraphsDir() const { return Resolve(config.paths.graphs); }
    std::filesystem::path WorkflowsDir() const { return Resolve(config.paths.workflows); }
    std::filesystem::path ScriptsDir() const { return Resolve(config.paths.scripts); }
    std::filesystem::path OutputDir() const { return Resolve(config.paths.output); }
    std::vector<std::filesystem::path> DataDirs() const;
    // The interpreter as something the shell can run (relative -> under root).
    std::string PythonExe() const;

    // Absolute path for a project-relative (or already absolute) path.
    std::filesystem::path Resolve(const std::string& relativeOrAbsolute) const;
    // Like Resolve, but a bare/relative name that does not exist under the
    // root is also looked for in each [paths].data folder. Returns the first
    // existing candidate, else Resolve(name).
    std::filesystem::path FindData(const std::string& name) const;
    // Like Resolve, relative to paths.scripts when the file is not under root.
    std::filesystem::path FindScript(const std::string& name) const;
    // Project-relative form of an absolute path when it lies under the root,
    // otherwise the absolute path unchanged.
    std::string Relativise(const std::filesystem::path& absolute) const;

    ProjectConfig config;

private:
    std::filesystem::path root;
};

// Serialisation (exposed for tests / tooling). The serialiser writes a
// commented, hand-editable file; the parser accepts anything TOML.
std::string SerialiseProjectConfig(const ProjectConfig& config);
bool ParseProjectConfig(const std::string& toml, ProjectConfig& out, std::string& error);
