#include "app/actions.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>

#include <fmt/format.h>

#include "core/math_utils.h"
#include "core/xyz_io.h"

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
void Log(AppState& state, LogLevel level, const std::string& text) {
    state.log.push_back({level, text, GetTime()});
    if (state.log.size() > 2000) state.log.pop_front();
}

// ---------------------------------------------------------------------------
// Projects
// ---------------------------------------------------------------------------
static std::string ColorToHex(Color c) { return fmt::format("#{:02x}{:02x}{:02x}", c.r, c.g, c.b); }

static void ClearSession(AppState& state) {
    state.structures.clear();
    state.activeStructure = -1;
    state.measurements.clear();
    state.selected.clear();
    CancelPendingMeasurement(state);
    state.model.Unload();
    state.modelDirty = true;
}

// Push the project's config onto the live session.
static void ApplyProjectToState(AppState& state) {
    const Project& p = *state.project;
    const ProjectConfig& c = p.config;

    RenderStyle style;
    if (ParseRenderStyle(c.view.style.c_str(), style)) state.render.style = style;
    Color bg;
    if (ParseColor({c.view.background}, 0, bg)) state.background = bg;
    state.drawGrid = c.view.grid;
    state.drawAtomNumbers = c.view.atomNumbers;
    state.render.ballScale = c.view.ballScale;
    state.render.stickRadius = c.view.stickRadius;
    state.render.sphereScale = c.view.sphereScale;
    state.twoDPlotIndex = c.view.twoDPlotIndex;
    state.calc.bondTolerance = c.view.bondTolerance;

    ClearSession(state);
    for (const ProjectStructureEntry& entry : c.structures) {
        const std::string path = p.Resolve(entry.path).string();
        CommandResult r = LoadStructureFile(state, path, false);
        if (!r.ok) continue;
        Structure& s = state.structures.back();
        if (!entry.name.empty()) s.name = entry.name;
        if (entry.frame >= 0 && entry.frame < (int)s.frames.nframes) s.activeFrame = entry.frame;
        if (state.calc.bondTolerance != 0.4f)
            for (Atoms& a : s.frames.atoms) a.covalentBondList = MakeCovalentBondList(a, state.calc.bondTolerance);
    }
    if (!state.structures.empty()) {
        const int active = std::clamp(c.activeStructure, 0, (int)state.structures.size() - 1);
        state.activeStructure = -1;
        SetActiveStructure(state, active);
        ResetCamera(state);
        // Measurements belong to the active structure's entry.
        if (active < (int)c.structures.size()) {
            for (const auto& m : c.structures[active].measurements) {
                std::vector<int> zeroBased;
                for (int i : m) zeroBased.push_back(i - 1);
                AddMeasurement(state, zeroBased);
            }
        }
    }
    state.pendingIniFile = p.LayoutPath().string();
    for (const std::string& cmd : c.startupCommands) {
        Log(state, LogLevel::Command, "> " + cmd);
        CommandResult r = state.commands.ExecuteScript(state, cmd);
        if (!r.message.empty()) Log(state, r.ok ? LogLevel::Result : LogLevel::Error, r.message);
    }
    state.projectDirty = false;
}

void CaptureProjectState(AppState& state) {
    if (!state.project) return;
    Project& p = *state.project;
    ProjectConfig& c = p.config;
    c.view.style = RenderStyleName(state.render.style);
    c.view.background = ColorToHex(state.background);
    c.view.grid = state.drawGrid;
    c.view.atomNumbers = state.drawAtomNumbers;
    c.view.ballScale = state.render.ballScale;
    c.view.stickRadius = state.render.stickRadius;
    c.view.sphereScale = state.render.sphereScale;
    c.view.twoDPlotIndex = state.twoDPlotIndex;
    c.view.bondTolerance = state.calc.bondTolerance;

    c.structures.clear();
    for (size_t i = 0; i < state.structures.size(); ++i) {
        const Structure& s = state.structures[i];
        if (s.path.empty()) continue;   // in-app structures have nothing on disk yet
        ProjectStructureEntry e;
        e.path = p.Relativise(s.path);
        e.name = s.name == std::filesystem::path(s.path).filename().string() ? "" : s.name;
        e.frame = s.activeFrame;
        if ((int)i == state.activeStructure)
            for (const Measurement& m : state.measurements) {
                std::vector<int> oneBased;
                for (int k = 0; k < m.count; ++k) oneBased.push_back(m.atoms[k] + 1);
                e.measurements.push_back(oneBased);
            }
        c.structures.push_back(std::move(e));
    }
    c.activeStructure = std::max(0, state.activeStructure);
}

std::string ProjectRelative(const AppState& state, const std::string& absolutePath) {
    if (!state.project) return absolutePath;
    return state.project->Relativise(absolutePath);
}

CommandResult NewProject(AppState& state, const std::string& directory, const std::string& name) {
    if (directory.empty()) return CommandResult::Error("usage: project new <directory> [name]");
    std::string error;
    auto project = Project::Create(directory, name, error);
    if (!project) return CommandResult::Error(error);
    // A new project adopts whatever is loaded right now.
    state.project = std::move(project);
    CaptureProjectState(state);
    if (!state.project->Save(error)) return CommandResult::Error(error);
    state.pendingIniFile = state.project->LayoutPath().string();
    state.projectDirty = false;
    const std::string msg = fmt::format("Created project '{}' at {}", state.project->config.name, state.project->Root().string());
    LogInfo(state, msg);
    return CommandResult::Ok(msg);
}

CommandResult OpenProject(AppState& state, const std::string& path) {
    if (path.empty()) return CommandResult::Error("usage: project open <directory or chemlab.toml>");
    std::string error;
    auto project = Project::Load(path, error);
    if (!project) return CommandResult::Error(error);
    state.project = std::move(project);
    ApplyProjectToState(state);
    const std::string msg = fmt::format("Opened project '{}' ({} structure{}) at {}", state.project->config.name,
                                        state.structures.size(), state.structures.size() == 1 ? "" : "s",
                                        state.project->Root().string());
    LogInfo(state, msg);
    return CommandResult::Ok(msg);
}

CommandResult SaveProject(AppState& state) {
    if (!state.project) return CommandResult::Error("No project open (use `project new <dir>`)");
    CaptureProjectState(state);
    std::string error;
    if (!state.project->Save(error)) return CommandResult::Error(error);
    state.projectDirty = false;
    const std::string msg = fmt::format("Saved {}", state.project->ConfigPath().string());
    LogInfo(state, msg);
    return CommandResult::Ok(msg);
}

CommandResult CloseProject(AppState& state) {
    if (!state.project) return CommandResult::Error("No project open");
    const std::string name = state.project->config.name;
    state.project.reset();
    ClearSession(state);
    state.pendingIniFile = "chemlab_imgui.ini";
    return CommandResult::Ok(fmt::format("Closed project '{}'", name));
}

// ---------------------------------------------------------------------------
// Structures / frames
// ---------------------------------------------------------------------------
static void OnActiveFrameChanged(AppState& state) {
    state.selected.clear();
    state.hoveredAtom = -1;
    CancelPendingMeasurement(state);
    state.modelDirty = true;
}

CommandResult LoadStructureFile(AppState& state, const std::string& path, bool makeActive) {
    Structure s;
    try {
        s.frames = ReadXYZ(path);
    } catch (const std::exception& e) {
        LogError(state, e.what());
        return CommandResult::Error(e.what());
    }
    s.path = std::filesystem::absolute(path).string();
    s.name = std::filesystem::path(path).filename().string();
    s.activeFrame = 0;
    state.structures.push_back(std::move(s));
    state.projectDirty = true;
    const int index = (int)state.structures.size() - 1;
    const auto& loaded = state.structures[index];
    const std::string msg = fmt::format("Loaded {} ({} frame{}, {} atoms)", loaded.name, loaded.frames.nframes,
                                        loaded.frames.nframes == 1 ? "" : "s", loaded.frames.atoms[0].natoms);
    LogInfo(state, msg);
    if (makeActive) {
        SetActiveStructure(state, index);
        ResetCamera(state);
    }
    return CommandResult::Ok(msg);
}

CommandResult SetActiveStructure(AppState& state, int index) {
    if (index < 0 || index >= (int)state.structures.size())
        return CommandResult::Error(fmt::format("No structure with index {}", index + 1));
    if (index == state.activeStructure) return CommandResult::Ok();
    state.activeStructure = index;
    state.measurements.clear();
    OnActiveFrameChanged(state);
    return CommandResult::Ok(fmt::format("Active structure: {}", state.structures[index].name));
}

CommandResult RemoveStructure(AppState& state, int index) {
    if (index < 0 || index >= (int)state.structures.size())
        return CommandResult::Error(fmt::format("No structure with index {}", index + 1));
    const std::string name = state.structures[index].name;
    state.structures.erase(state.structures.begin() + index);
    state.projectDirty = true;
    if (state.structures.empty()) {
        state.activeStructure = -1;
        state.model.Unload();
    } else if (state.activeStructure >= (int)state.structures.size()) {
        state.activeStructure = (int)state.structures.size() - 1;
    } else if (index < state.activeStructure) {
        state.activeStructure--;
    }
    state.measurements.clear();
    OnActiveFrameChanged(state);
    return CommandResult::Ok(fmt::format("Removed {}", name));
}

CommandResult RenameStructure(AppState& state, int index, const std::string& name) {
    if (index < 0 || index >= (int)state.structures.size())
        return CommandResult::Error(fmt::format("No structure with index {}", index + 1));
    state.structures[index].name = name;
    return CommandResult::Ok();
}

CommandResult SetFrame(AppState& state, int frameIndex) {
    Structure* s = state.ActiveStructure();
    if (!s) return CommandResult::Error("No structure loaded");
    if (frameIndex < 0 || frameIndex >= (int)s->frames.nframes)
        return CommandResult::Error(fmt::format("Frame {} out of range 1..{}", frameIndex + 1, s->frames.nframes));
    if (frameIndex == s->activeFrame) return CommandResult::Ok();
    // Keep the selection across frames of the same size: comparing frames of a
    // trajectory is the whole point of stepping through them.
    const bool sameSize = s->frames.atoms[frameIndex].natoms == s->frames.atoms[s->activeFrame].natoms;
    s->activeFrame = frameIndex;
    if (sameSize) {
        state.modelDirty = true;
    } else {
        state.measurements.clear();
        OnActiveFrameChanged(state);
    }
    return CommandResult::Ok(fmt::format("Frame {}/{}", frameIndex + 1, s->frames.nframes));
}

CommandResult StepFrame(AppState& state, int delta) {
    Structure* s = state.ActiveStructure();
    if (!s) return CommandResult::Error("No structure loaded");
    int next = s->activeFrame + delta;
    const int n = (int)s->frames.nframes;
    if (state.playback.loop) next = ((next % n) + n) % n;
    else next = std::clamp(next, 0, n - 1);
    return SetFrame(state, next);
}

void UpdatePlayback(AppState& state) {
    if (!state.playback.playing) return;
    const Structure* s = state.ActiveStructure();
    if (!s || s->frames.nframes < 2) { state.playback.playing = false; return; }
    const double now = GetTime();
    const double period = 1.0 / std::fmax(state.playback.framesPerSecond, 0.01f);
    if (now - state.playback.lastAdvance >= period) {
        state.playback.lastAdvance = now;
        if (!state.playback.loop && s->activeFrame + 1 >= (int)s->frames.nframes) {
            state.playback.playing = false;
            return;
        }
        StepFrame(state, +1);
    }
}

void UpdateFileWatch(AppState& state) {
    if (!state.watchFiles) return;
    static double lastCheck = 0.0;
    const double now = GetTime();
    if (now - lastCheck < 0.5) return;   // stat()ing every frame is wasteful
    lastCheck = now;
    for (size_t i = 0; i < state.structures.size(); ++i) {
        Structure& s = state.structures[i];
        if (s.path.empty()) continue;
        if (CheckForFileChangesAndUpdate(s.frames)) {
            s.activeFrame = std::clamp(s.activeFrame, 0, (int)s.frames.nframes - 1);
            LogInfo(state, fmt::format("Reloaded {} ({} frames)", s.name, s.frames.nframes));
            if ((int)i == state.activeStructure) {
                state.measurements.clear();
                OnActiveFrameChanged(state);
            }
        }
    }
}

void RebuildModel(AppState& state) {
    state.modelDirty = false;
    Atoms* atoms = state.ActiveAtoms();
    if (!atoms) { state.model.Unload(); return; }
    // Preserve custom colours when the atom count is unchanged (trajectory).
    if (state.model.IsLoaded() && state.model.AtomCount() == atoms->natoms) {
        state.model.UpdateGeometry(*atoms, state.render);
    } else {
        state.model.Build(*atoms, state.render);
    }
}

void MarkGeometryChanged(AppState& state) { state.modelDirty = true; }

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
void ResetCamera(AppState& state) {
    const Atoms* atoms = state.ActiveAtoms();
    if (!atoms || atoms->natoms == 0) {
        state.viewport.orbit.Reset(Vector3Zero(), 20.0f);
        return;
    }
    Vector3 lo{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vector3 hi{-lo.x, -lo.y, -lo.z};
    for (const Vector3& p : atoms->xyz) {
        lo = Vector3Min(lo, p);
        hi = Vector3Max(hi, p);
    }
    state.viewport.orbit.FrameBounds(lo - Vector3{1, 1, 1}, hi + Vector3{1, 1, 1});
}

void LookDownAxis(AppState& state, int axis, bool flip) { state.viewport.orbit.LookDownAxis(axis, flip); }

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------
static CommandResult SelectionSummary(const AppState& state) {
    return CommandResult::Ok(fmt::format("{} atom{} selected", state.selected.size(), state.selected.size() == 1 ? "" : "s"));
}

CommandResult SelectAtoms(AppState& state, const std::set<int>& atoms, bool add) {
    const Atoms* a = state.ActiveAtoms();
    if (!a) return CommandResult::Error("No structure loaded");
    if (!add) state.selected.clear();
    for (int i : atoms) {
        if (i < 0 || i >= (int)a->natoms)
            return CommandResult::Error(fmt::format("Atom {} out of range 1..{}", i + 1, a->natoms));
        state.selected.insert(i);
    }
    return SelectionSummary(state);
}

CommandResult SelectByElement(AppState& state, const std::string& element, bool add) {
    const Atoms* a = state.ActiveAtoms();
    if (!a) return CommandResult::Error("No structure loaded");
    if (!add) state.selected.clear();
    for (uint32_t i = 0; i < a->natoms; ++i)
        if (a->labels[i] == element) state.selected.insert((int)i);
    return SelectionSummary(state);
}

CommandResult SelectAll(AppState& state) {
    const Atoms* a = state.ActiveAtoms();
    if (!a) return CommandResult::Error("No structure loaded");
    for (uint32_t i = 0; i < a->natoms; ++i) state.selected.insert((int)i);
    return SelectionSummary(state);
}

CommandResult SelectNone(AppState& state) {
    state.selected.clear();
    return SelectionSummary(state);
}

CommandResult InvertSelection(AppState& state) {
    const Atoms* a = state.ActiveAtoms();
    if (!a) return CommandResult::Error("No structure loaded");
    std::set<int> inverted;
    for (uint32_t i = 0; i < a->natoms; ++i)
        if (!state.selected.count((int)i)) inverted.insert((int)i);
    state.selected = std::move(inverted);
    return SelectionSummary(state);
}

CommandResult ToggleAtomSelected(AppState& state, int atom) {
    if (state.selected.count(atom)) state.selected.erase(atom);
    else state.selected.insert(atom);
    if (state.selected.count(atom) && atom < (int)state.model.atomColors.size()) {
        const Color c = state.model.atomColors[atom];
        state.pickerColor[0] = c.r / 255.0f; state.pickerColor[1] = c.g / 255.0f;
        state.pickerColor[2] = c.b / 255.0f; state.pickerColor[3] = c.a / 255.0f;
    }
    return SelectionSummary(state);
}

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------
CommandResult ColorSelection(AppState& state, Color color) {
    if (state.modelDirty) RebuildModel(state);
    if (!state.model.IsLoaded()) return CommandResult::Error("No structure loaded");
    if (state.selected.empty()) return CommandResult::Error("Nothing selected (shift-click atoms or use `select`)");
    for (int i : state.selected)
        if (i >= 0 && i < (int)state.model.atomColors.size()) state.model.atomColors[i] = color;
    return CommandResult::Ok(fmt::format("Coloured {} atoms", state.selected.size()));
}

CommandResult SetSelectionAlpha(AppState& state, float alpha) {
    if (state.modelDirty) RebuildModel(state);
    if (!state.model.IsLoaded()) return CommandResult::Error("No structure loaded");
    if (state.selected.empty()) return CommandResult::Error("Nothing selected");
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    for (int i : state.selected)
        if (i >= 0 && i < (int)state.model.atomColors.size()) state.model.atomColors[i].a = (unsigned char)(alpha * 255.0f);
    return CommandResult::Ok(fmt::format("Alpha {:.2f} on {} atoms", alpha, state.selected.size()));
}

CommandResult ResetColors(AppState& state) {
    const Atoms* a = state.ActiveAtoms();
    if (!a || !state.model.IsLoaded()) return CommandResult::Error("No structure loaded");
    for (uint32_t i = 0; i < a->natoms && i < state.model.atomColors.size(); ++i)
        state.model.atomColors[i] = a->renderData[i].color;
    return CommandResult::Ok("Colours reset to element defaults");
}

// ---------------------------------------------------------------------------
// Measurements
// ---------------------------------------------------------------------------
void MeasurementClick(AppState& state, int atom) {
    for (int i = 0; i < state.pendingCount; ++i)
        if (state.pendingMeasurement[i] == atom) return;  // ignore repeats
    state.pendingMeasurement[state.pendingCount++] = atom;
    if (state.pendingCount == 4) CommitPendingMeasurement(state);
}

void CommitPendingMeasurement(AppState& state) {
    if (state.pendingCount >= 2) {
        Measurement m;
        m.atoms = state.pendingMeasurement;
        m.count = state.pendingCount;
        state.measurements.push_back(m);
        state.measurementsVersion++;
        const Atoms* a = state.ActiveAtoms();
        if (a) LogInfo(state, fmt::format("{} = {:.4f}", MeasurementLabel(m), MeasurementValue(*a, m)));
    }
    CancelPendingMeasurement(state);
}

void CancelPendingMeasurement(AppState& state) {
    state.pendingMeasurement.fill(-1);
    state.pendingCount = 0;
}

CommandResult AddMeasurement(AppState& state, const std::vector<int>& atoms) {
    const Atoms* a = state.ActiveAtoms();
    if (!a) return CommandResult::Error("No structure loaded");
    if (atoms.size() < 2 || atoms.size() > 4) return CommandResult::Error("measure needs 2, 3 or 4 atoms");
    Measurement m;
    m.count = (int)atoms.size();
    for (size_t i = 0; i < atoms.size(); ++i) {
        if (atoms[i] < 0 || atoms[i] >= (int)a->natoms)
            return CommandResult::Error(fmt::format("Atom {} out of range 1..{}", atoms[i] + 1, a->natoms));
        m.atoms[i] = atoms[i];
    }
    state.measurements.push_back(m);
    state.measurementsVersion++;
    return CommandResult::Ok(fmt::format("{} = {:.4f}", MeasurementLabel(m), MeasurementValue(*a, m)));
}

CommandResult RemoveMeasurement(AppState& state, int index) {
    if (index < 0 || index >= (int)state.measurements.size()) return CommandResult::Error("No such measurement");
    state.measurements.erase(state.measurements.begin() + index);
    state.measurementsVersion++;
    return CommandResult::Ok();
}

CommandResult ClearMeasurements(AppState& state) {
    const size_t n = state.measurements.size();
    state.measurements.clear();
    state.measurementsVersion++;
    CancelPendingMeasurement(state);
    return CommandResult::Ok(fmt::format("Cleared {} measurement{}", n, n == 1 ? "" : "s"));
}

double MeasurementValue(const Atoms& atoms, const Measurement& m) {
    for (int i = 0; i < m.count; ++i)
        if (m.atoms[i] < 0 || m.atoms[i] >= (int)atoms.natoms) return std::numeric_limits<double>::quiet_NaN();
    const auto& p = atoms.xyz;
    switch (m.count) {
        case 2: return Distance(p[m.atoms[0]], p[m.atoms[1]]);
        case 3: return AngleDeg(p[m.atoms[0]], p[m.atoms[1]], p[m.atoms[2]]);
        case 4: return DihedralDeg(p[m.atoms[0]], p[m.atoms[1]], p[m.atoms[2]], p[m.atoms[3]]);
        default: return std::numeric_limits<double>::quiet_NaN();
    }
}

std::string MeasurementLabel(const Measurement& m) {
    const char* kind = m.count == 2 ? "d" : m.count == 3 ? "angle" : "dihedral";
    std::string s = fmt::format("{}(", kind);
    for (int i = 0; i < m.count; ++i) s += fmt::format("{}{}", i ? "," : "", m.atoms[i] + 1);
    return s + ")";
}

// ---------------------------------------------------------------------------
// Calculations
// ---------------------------------------------------------------------------
CommandResult RecomputeBonds(AppState& state) {
    Structure* s = state.ActiveStructure();
    if (!s) return CommandResult::Error("No structure loaded");
    size_t total = 0;
    for (Atoms& a : s->frames.atoms) {
        a.covalentBondList = MakeCovalentBondList(a, state.calc.bondTolerance);
        total += a.covalentBondList.pairs.size();
    }
    MarkGeometryChanged(state);
    return CommandResult::Ok(fmt::format("Recomputed bonds (tolerance {:.2f} A): {} bonds over {} frames",
                                         state.calc.bondTolerance, total, s->frames.nframes));
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------
CommandResult SaveScreenshot(AppState& state, const std::string& pathIn, int width, int height, bool transparent) {
    if (state.modelDirty) RebuildModel(state);
    if (!state.model.IsLoaded()) return CommandResult::Error("No structure loaded");
    std::filesystem::path path = pathIn.empty() ? "screenshot.png" : pathIn;
    if (path.extension().empty()) path.replace_extension(".png");
    ViewportScene scene;
    scene.model = &state.model;
    scene.highlighted = nullptr;   // no selection rings in exported images
    scene.settings = &state.render;
    scene.drawGrid = false;
    scene.background = state.background;
    Image image = state.viewport.RenderToImage(scene, width, height, transparent);
    const bool ok = ExportImage(image, path.string().c_str());
    UnloadImage(image);
    if (!ok) return CommandResult::Error("Failed to write " + path.string());
    state.exportSettings.lastScreenshotPath = path.string();
    const std::string msg = fmt::format("Saved {} ({}x{})", path.string(), width, height);
    LogInfo(state, msg);
    return CommandResult::Ok(msg);
}

CommandResult ExportXYZ(AppState& state, const std::string& pathIn, bool allFrames) {
    Structure* s = state.ActiveStructure();
    if (!s) return CommandResult::Error("No structure loaded");
    std::filesystem::path path = pathIn.empty() ? "export.xyz" : pathIn;
    if (path.extension().empty()) path.replace_extension(".xyz");
    if (!WriteXYZ(path.string(), s->frames, allFrames ? -1 : s->activeFrame))
        return CommandResult::Error("Failed to write " + path.string());
    state.exportSettings.lastXYZPath = path.string();
    const std::string msg = fmt::format("Wrote {} ({})", path.string(), allFrames ? "all frames" : "current frame");
    LogInfo(state, msg);
    return CommandResult::Ok(msg);
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static const std::map<std::string, Color> kNamedColors = {
    {"red", RED},       {"green", GREEN},   {"blue", BLUE},     {"yellow", YELLOW},
    {"orange", ORANGE}, {"purple", PURPLE}, {"violet", VIOLET}, {"pink", PINK},
    {"white", WHITE},   {"black", BLACK},   {"gray", GRAY},     {"grey", GRAY},
    {"lightgray", LIGHTGRAY}, {"darkgray", DARKGRAY}, {"skyblue", SKYBLUE}, {"lime", LIME},
    {"gold", GOLD},     {"maroon", MAROON}, {"brown", BROWN},   {"beige", BEIGE},
    {"magenta", MAGENTA}, {"darkblue", DARKBLUE}, {"darkgreen", DARKGREEN},
};

bool ParseColor(const std::vector<std::string>& tokens, size_t start, Color& out) {
    if (start >= tokens.size()) return false;
    const std::string& first = tokens[start];
    if (auto it = kNamedColors.find(first); it != kNamedColors.end()) { out = it->second; return true; }
    if (first.size() >= 7 && first[0] == '#') {
        unsigned int v = 0;
        try { v = std::stoul(first.substr(1), nullptr, 16); } catch (...) { return false; }
        if (first.size() == 7) { out = Color{(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, 255}; return true; }
        if (first.size() == 9) { out = Color{(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v}; return true; }
        return false;
    }
    if (tokens.size() < start + 3) return false;
    try {
        float c[4] = {std::stof(tokens[start]), std::stof(tokens[start + 1]), std::stof(tokens[start + 2]), 1.0f};
        if (tokens.size() > start + 3) c[3] = std::stof(tokens[start + 3]);
        // Accept either 0..1 floats or 0..255 ints.
        const bool isUnit = c[0] <= 1.0f && c[1] <= 1.0f && c[2] <= 1.0f;
        const float scale = isUnit ? 255.0f : 1.0f;
        out = Color{(unsigned char)std::clamp(c[0] * scale, 0.0f, 255.0f), (unsigned char)std::clamp(c[1] * scale, 0.0f, 255.0f),
                    (unsigned char)std::clamp(c[2] * scale, 0.0f, 255.0f), (unsigned char)std::clamp(c[3] * (c[3] <= 1.0f ? 255.0f : 1.0f), 0.0f, 255.0f)};
        return true;
    } catch (...) {
        return false;
    }
}

Color ImVec4ToColor(const float rgba[4]) {
    return Color{(unsigned char)(rgba[0] * 255.0f), (unsigned char)(rgba[1] * 255.0f), (unsigned char)(rgba[2] * 255.0f),
                 (unsigned char)(rgba[3] * 255.0f)};
}
