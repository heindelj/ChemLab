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
// The file is meant to be read and edited by people, so it is written by hand
// with comments rather than dumped from a toml::table (which would drop them).
namespace {

// Floats are stored as doubles by TOML; round so 0.25f does not become
// 0.25000000372529030 in a file people edit by hand.
double Tidy(float v) { return std::round((double)v * 1e4) / 1e4; }

std::string Q(const std::string& s) {   // a double-quoted, escaped TOML string
    std::string o = "\"";
    for (unsigned char ch : s) {
        switch (ch) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\t': o += "\\t"; break;
            case '\r': o += "\\r"; break;
            default:
                if (ch < 0x20) o += fmt::format("\\u{:04X}", ch);
                else o += (char)ch;
        }
    }
    return o + "\"";
}

std::string QList(const std::vector<std::string>& v) {
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i) o += (i ? ", " : "") + Q(v[i]);
    return o + "]";
}

std::string F(float v) {   // a TOML float: always carries a decimal point
    std::string s = fmt::format("{}", Tidy(v));
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    return s;
}

const char* B(bool b) { return b ? "true" : "false"; }

}  // namespace

std::string SerialiseProjectConfig(const ProjectConfig& c) {
    std::string out;
    auto line = [&](std::string s) { out += s; out += '\n'; };

    line("# ChemLab project.");
    line("# All paths are relative to the folder containing this file, which is the");
    line("# working directory while the project is open (so bare file names in `load`,");
    line("# `screenshot`, `export`, scripts and file dialogs resolve here).");
    line("");
    line("[project]");
    line(fmt::format("name = {}", Q(c.name)));
    line(fmt::format("description = {}", Q(c.description)));
    line(fmt::format("format = {}                     # schema version, for future migrations", c.format));
    line(fmt::format("active_structure = {}           # 1-based index into [[structures]]", c.activeStructure + 1));
    line("");
    line("# Where the project keeps things; each folder is created when missing. `data`");
    line("# is a list so a project can also point at folders outside itself (e.g. a big");
    line("# read-only trajectory store): bare structure names are searched there in order.");
    line("[paths]");
    line(fmt::format("data    = {}", QList(c.paths.data)));
    line(fmt::format("scenes  = {}             # scene graphs (layouts + panels + tabs), one .json each", Q(c.paths.scenes)));
    line(fmt::format("graphs  = {}             # named node graphs from the Graph Canvas", Q(c.paths.graphs)));
    line(fmt::format("workflows = {}         # node-graph workflows run by the executor (Workflows panel)", Q(c.paths.workflows)));
    line(fmt::format("scripts = {}            # python node scripts / analysis scripts", Q(c.paths.scripts)));
    line(fmt::format("output  = {}             # screenshots, exports, plot images", Q(c.paths.output)));
    line(fmt::format("layout  = {}         # Dear ImGui dock state for this project", Q(c.paths.layout)));
    line("");
    line("# The scene (and which of its layouts) shown when the project opens: a");
    line("# built-in (\"classic\", \"plot-lab\") or a file in the scenes folder. Updated");
    line("# when you switch scenes; saved with the project.");
    line("[scene]");
    line(fmt::format("active = {}", Q(c.scene.active)));
    line(fmt::format("layout = {}", Q(c.scene.layout)));
    line("");
    line("[view]");
    line(fmt::format("style          = {}   # ball-and-stick | sticks | spheres | wireframe", Q(c.view.style)));
    line(fmt::format("background     = {}", Q(c.view.background)));
    line(fmt::format("grid           = {}", B(c.view.grid)));
    line(fmt::format("atom_numbers   = {}", B(c.view.atomNumbers)));
    line(fmt::format("ball_scale     = {}", F(c.view.ballScale)));
    line(fmt::format("stick_radius   = {}", F(c.view.stickRadius)));
    line(fmt::format("sphere_scale   = {}", F(c.view.sphereScale)));
    line(fmt::format("plot           = {}           # energy | measurements", Q(c.view.twoDPlotIndex == 1 ? "measurements" : "energy")));
    line(fmt::format("bond_tolerance = {}", F(c.view.bondTolerance)));
    line("");
    line("# Interpreter used for python script nodes in this project. A relative path");
    line("# (e.g. \".venv/bin/python\") is resolved against the project root; `env` adds");
    line("# environment variables when scripts run.");
    line("[python]");
    line(fmt::format("interpreter = {}", Q(c.python.interpreter)));
    {
        // Inline table form: { A = "1", B = "2" }
        std::string inl = "{";
        bool first = true;
        for (const auto& [k, v] : c.python.env) {
            inl += first ? " " : ", ";
            first = false;
            const bool bare = !k.empty() && k.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos;
            inl += fmt::format("{} = {}", bare ? k : Q(k), Q(v));
        }
        inl += first ? "}" : " }";
        line(fmt::format("env = {}", inl));
    }
    line("");
    line("# Project scripts, relative to paths.scripts. Script nodes whose path is not");
    line("# found as written are looked for there too, so graphs can name scripts bare.");
    line("[scripts]");
    line(fmt::format("files = {}", QList(c.scripts)));
    line("");
    line("# Command-bar commands run after everything above is loaded.");
    line("[startup]");
    line(fmt::format("commands = {}", QList(c.startupCommands)));
    line("");
    line("# Loaded structures. `path` is relative to the project root or to any of the");
    line("# paths.data folders (searched in order). `frame` and `measurements` (atom");
    line("# indices, 2 to 4 per measurement) are 1-based.");
    if (c.structures.empty()) {
        line("#");
        line("# [[structures]]");
        line("# path = \"caffeine.xyz\"");
        line("# name = \"caffeine\"");
        line("# frame = 1");
        line("# measurements = [[1, 2], [1, 2, 3]]");
    }
    for (const ProjectStructureEntry& s : c.structures) {
        line("");
        line("[[structures]]");
        line(fmt::format("path = {}", Q(s.path)));
        if (!s.name.empty()) line(fmt::format("name = {}", Q(s.name)));
        if (s.frame > 0) line(fmt::format("frame = {}", s.frame + 1));
        if (!s.measurements.empty()) {
            std::string ms = "[";
            for (size_t i = 0; i < s.measurements.size(); ++i) {
                if (i) ms += ", ";
                ms += "[";
                for (size_t k = 0; k < s.measurements[i].size(); ++k)
                    ms += fmt::format("{}{}", k ? ", " : "", s.measurements[i][k]);
                ms += "]";
            }
            ms += "]";
            line(fmt::format("measurements = {}", ms));
        }
    }
    return out;
}

namespace {

template <typename NodeView>
std::vector<std::string> StringList(const NodeView& n) {
    std::vector<std::string> out;
    if (auto s = n.template value<std::string>()) { out.push_back(*s); return out; }
    if (const toml::array* arr = n.as_array())
        for (const toml::node& e : *arr)
            if (auto v = e.value<std::string>()) out.push_back(*v);
    return out;
}

}  // namespace

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
        c.format = (*p)["format"].value_or(ProjectConfig::kFormat);
        c.activeStructure = (*p)["active_structure"].value_or(1) - 1;
        // Older files kept the layout file name here.
        if (auto legacy = (*p)["layout"].value<std::string>()) c.paths.layout = *legacy;
    }
    if (c.format > ProjectConfig::kFormat) {
        error = fmt::format("project format {} is newer than this ChemLab understands ({})", c.format, ProjectConfig::kFormat);
        return false;
    }
    if (const toml::table* p = root["paths"].as_table()) {
        const ProjectPaths d;
        if (p->contains("data")) c.paths.data = StringList((*p)["data"]);
        c.paths.scenes = (*p)["scenes"].value_or(d.scenes);
        c.paths.graphs = (*p)["graphs"].value_or(d.graphs);
        c.paths.workflows = (*p)["workflows"].value_or(d.workflows);
        c.paths.scripts = (*p)["scripts"].value_or(d.scripts);
        c.paths.output = (*p)["output"].value_or(d.output);
        c.paths.layout = (*p)["layout"].value_or(c.paths.layout);
    }
    if (const toml::table* s = root["scene"].as_table()) {
        const ProjectSceneSettings d;
        c.scene.active = (*s)["active"].value_or(d.active);
        c.scene.layout = (*s)["layout"].value_or(d.layout);
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
        c.view.twoDPlotIndex = (*v)["plot"].value_or(std::string{"energy"}) == "measurements" ? 1 : 0;
        c.view.bondTolerance = (float)(*v)["bond_tolerance"].value_or((double)d.bondTolerance);
    }
    if (const toml::table* py = root["python"].as_table()) {
        const ProjectPython d;
        c.python.interpreter = (*py)["interpreter"].value_or(d.interpreter);
        if (const toml::table* env = (*py)["env"].as_table())
            for (const auto& [k, v] : *env)
                if (auto s = v.value<std::string>()) c.python.env.emplace_back(std::string(k.str()), *s);
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
    c.startupCommands = StringList(root["startup"]["commands"]);
    c.scripts = StringList(root["scripts"]["files"]);
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
    project.EnsureFolders();
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

void Project::EnsureFolders() const {
    std::error_code ec;
    for (const fs::path& d : DataDirs())
        if (!d.empty() && Relativise(d) != d.string()) fs::create_directories(d, ec);   // only folders inside the project
    fs::create_directories(ScenesDir(), ec);
    fs::create_directories(GraphsDir(), ec);
    fs::create_directories(WorkflowsDir(), ec);
    fs::create_directories(ScriptsDir(), ec);
    fs::create_directories(OutputDir(), ec);
}

std::vector<fs::path> Project::DataDirs() const {
    std::vector<fs::path> dirs;
    for (const std::string& d : config.paths.data) dirs.push_back(Resolve(d));
    return dirs;
}

std::string Project::PythonExe() const {
    const std::string& py = config.python.interpreter;
    if (py.empty()) return "python3";
    // A bare command ("python3") is left to PATH; anything with a separator is a file.
    if (py.find('/') == std::string::npos && py.find('\\') == std::string::npos) return py;
    return Resolve(py).string();
}

fs::path Project::Resolve(const std::string& p) const {
    const fs::path path = p;
    return (path.is_absolute() ? path : root / path).lexically_normal();
}

fs::path Project::FindData(const std::string& name) const {
    std::error_code ec;
    const fs::path direct = Resolve(name);
    if (fs::path(name).is_absolute() || fs::exists(direct, ec)) return direct;
    for (const fs::path& dir : DataDirs()) {
        const fs::path candidate = (dir / name).lexically_normal();
        if (fs::exists(candidate, ec)) return candidate;
    }
    return direct;
}

fs::path Project::FindScript(const std::string& name) const {
    std::error_code ec;
    const fs::path direct = Resolve(name);
    if (fs::path(name).is_absolute() || fs::exists(direct, ec)) return direct;
    const fs::path candidate = (ScriptsDir() / name).lexically_normal();
    if (fs::exists(candidate, ec)) return candidate;
    return direct;
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
