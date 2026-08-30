#include "app/project.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include <fmt/format.h>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------
// Floats are stored as doubles by TOML; round so 0.25f does not become
// 0.25000000372529030 in a file people edit by hand.
static double Tidy(float v) { return std::round((double)v * 1e4) / 1e4; }

std::string SerialiseProjectConfig(const ProjectConfig& c) {
    toml::table project{
        {"name", c.name},
        {"description", c.description},
        {"layout", c.layoutFile},
        {"active_structure", c.activeStructure + 1},
    };

    toml::table view{
        {"style", c.view.style},
        {"background", c.view.background},
        {"grid", c.view.grid},
        {"atom_numbers", c.view.atomNumbers},
        {"ball_scale", Tidy(c.view.ballScale)},
        {"stick_radius", Tidy(c.view.stickRadius)},
        {"sphere_scale", Tidy(c.view.sphereScale)},
        {"plot_pane_fraction", Tidy(c.view.twoDPaneFraction)},
        {"plot", c.view.twoDPlotIndex == 1 ? "measurements" : "energy"},
        {"bond_tolerance", Tidy(c.view.bondTolerance)},
    };

    toml::array structures;
    for (const ProjectStructureEntry& s : c.structures) {
        toml::table t{{"path", s.path}};
        if (!s.name.empty()) t.insert("name", s.name);
        if (s.frame > 0) t.insert("frame", s.frame + 1);
        if (!s.measurements.empty()) {
            toml::array ms;
            for (const auto& m : s.measurements) {
                toml::array idx;
                for (int i : m) idx.push_back(i);
                ms.push_back(idx);
            }
            t.insert("measurements", ms);
        }
        structures.push_back(t);
    }

    toml::array startup;
    for (const auto& cmd : c.startupCommands) startup.push_back(cmd);
    toml::array scripts;
    for (const auto& s : c.scripts) scripts.push_back(s);

    toml::table root{
        {"project", project},
        {"view", view},
        {"structures", structures},
        {"startup", toml::table{{"commands", startup}}},
        {"scripts", toml::table{{"files", scripts}}},
    };

    std::ostringstream out;
    out << "# ChemLab project. Paths are relative to this file's folder.\n" << root << "\n";
    return out.str();
}

bool ParseProjectConfig(const std::string& text, ProjectConfig& c, std::string& error) {
    toml::table root;
    try {
        root = toml::parse(text);
    } catch (const toml::parse_error& e) {
        std::ostringstream oss;
        oss << e;
        error = oss.str();
        return false;
    }
    c = ProjectConfig{};

    if (const toml::table* p = root["project"].as_table()) {
        c.name = (*p)["name"].value_or(std::string{});
        c.description = (*p)["description"].value_or(std::string{});
        c.layoutFile = (*p)["layout"].value_or(std::string{"layout.ini"});
        c.activeStructure = (*p)["active_structure"].value_or(1) - 1;
    }
    if (const toml::table* v = root["view"].as_table()) {
        ProjectViewSettings d;
        c.view.style = (*v)["style"].value_or(d.style);
        c.view.background = (*v)["background"].value_or(d.background);
        c.view.grid = (*v)["grid"].value_or(d.grid);
        c.view.atomNumbers = (*v)["atom_numbers"].value_or(d.atomNumbers);
        c.view.ballScale = (float)(*v)["ball_scale"].value_or((double)d.ballScale);
        c.view.stickRadius = (float)(*v)["stick_radius"].value_or((double)d.stickRadius);
        c.view.sphereScale = (float)(*v)["sphere_scale"].value_or((double)d.sphereScale);
        c.view.twoDPaneFraction = (float)(*v)["plot_pane_fraction"].value_or((double)d.twoDPaneFraction);
        c.view.twoDPlotIndex = (*v)["plot"].value_or(std::string{"energy"}) == "measurements" ? 1 : 0;
        c.view.bondTolerance = (float)(*v)["bond_tolerance"].value_or((double)d.bondTolerance);
    }
    if (const toml::array* arr = root["structures"].as_array()) {
        for (const toml::node& n : *arr) {
            const toml::table* t = n.as_table();
            if (!t) continue;
            ProjectStructureEntry s;
            s.path = (*t)["path"].value_or(std::string{});
            if (s.path.empty()) { error = "[[structures]] entry without a path"; return false; }
            s.name = (*t)["name"].value_or(std::string{});
            s.frame = (*t)["frame"].value_or(1) - 1;
            if (const toml::array* ms = (*t)["measurements"].as_array()) {
                for (const toml::node& mn : *ms) {
                    const toml::array* idx = mn.as_array();
                    if (!idx) continue;
                    std::vector<int> m;
                    for (const toml::node& in : *idx)
                        if (auto v = in.value<int64_t>()) m.push_back((int)*v);
                    if (m.size() >= 2 && m.size() <= 4) s.measurements.push_back(m);
                }
            }
            c.structures.push_back(std::move(s));
        }
    }
    if (const toml::array* cmds = root["startup"]["commands"].as_array())
        for (const toml::node& n : *cmds)
            if (auto v = n.value<std::string>()) c.startupCommands.push_back(*v);
    if (const toml::array* files = root["scripts"]["files"].as_array())
        for (const toml::node& n : *files)
            if (auto v = n.value<std::string>()) c.scripts.push_back(*v);
    return true;
}

// ---------------------------------------------------------------------------
// Project
// ---------------------------------------------------------------------------
bool Project::LooksLikeProject(const fs::path& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) return fs::exists(path / kConfigFileName, ec);
    return path.extension() == ".toml" && fs::exists(path, ec);
}

std::optional<Project> Project::Load(const fs::path& pathIn, std::string& error) {
    std::error_code ec;
    fs::path configPath = fs::is_directory(pathIn, ec) ? pathIn / kConfigFileName : pathIn;
    if (!fs::exists(configPath, ec)) {
        error = fmt::format("No {} found at {}", kConfigFileName, pathIn.string());
        return std::nullopt;
    }
    std::ifstream in(configPath);
    std::stringstream buffer;
    buffer << in.rdbuf();

    Project project;
    project.root = fs::absolute(configPath).parent_path().lexically_normal();
    if (!ParseProjectConfig(buffer.str(), project.config, error)) {
        error = fmt::format("{}: {}", configPath.string(), error);
        return std::nullopt;
    }
    if (project.config.name.empty()) project.config.name = project.root.filename().string();
    return project;
}

std::optional<Project> Project::Create(const fs::path& dir, const std::string& name, std::string& error) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        error = fmt::format("Could not create {}: {}", dir.string(), ec.message());
        return std::nullopt;
    }
    Project project;
    project.root = fs::absolute(dir).lexically_normal();
    project.config.name = name.empty() ? project.root.filename().string() : name;
    if (fs::exists(project.ConfigPath(), ec)) {
        error = fmt::format("{} already exists", project.ConfigPath().string());
        return std::nullopt;
    }
    if (!project.Save(error)) return std::nullopt;
    return project;
}

bool Project::Save(std::string& error) const {
    std::ofstream out(ConfigPath());
    if (!out) {
        error = fmt::format("Could not write {}", ConfigPath().string());
        return false;
    }
    out << SerialiseProjectConfig(config);
    return static_cast<bool>(out);
}

fs::path Project::Resolve(const std::string& p) const {
    const fs::path path = p;
    return (path.is_absolute() ? path : root / path).lexically_normal();
}

std::string Project::Relativise(const fs::path& absolute) const {
    std::error_code ec;
    const fs::path rel = fs::relative(absolute, root, ec);
    if (ec || rel.empty()) return absolute.string();
    // Keep paths that escape the root absolute: "../../elsewhere" breaks the
    // moment the project folder moves.
    if (!rel.empty() && *rel.begin() == "..") return absolute.string();
    return rel.generic_string();
}
