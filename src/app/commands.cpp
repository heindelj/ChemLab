#include "app/commands.h"

#include "graph/graph_system.h"

#include <algorithm>
#include <sstream>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "app/actions.h"
#include "app/app_state.h"
#include "ui/theme.h"

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
void CommandRegistry::Register(CommandSpec spec) {
    const std::string name = spec.name;
    commands[name] = std::move(spec);
}

const CommandSpec* CommandRegistry::Find(const std::string& name) const {
    auto it = commands.find(name);
    return it == commands.end() ? nullptr : &it->second;
}

std::vector<std::string> CommandRegistry::Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    char quote = 0;
    bool haveToken = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == quote) { inQuotes = false; }
            else if (c == '\\' && i + 1 < line.size()) { current += line[++i]; }
            else current += c;
        } else if (c == '"' || c == '\'') {
            inQuotes = true; quote = c; haveToken = true;
        } else if (c == '#' && !haveToken && (i + 1 >= line.size() || isspace((unsigned char)line[i + 1]))) {
            break;  // "# comment" (a bare '#ff8800' is a colour, not a comment)
        } else if (isspace((unsigned char)c)) {
            if (haveToken) { tokens.push_back(current); current.clear(); haveToken = false; }
        } else {
            current += c; haveToken = true;
        }
    }
    if (haveToken) tokens.push_back(current);
    return tokens;
}

CommandArgs CommandRegistry::ParseArgs(const std::vector<std::string>& tokens) {
    CommandArgs args;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        if (t.size() > 2 && t[0] == '-' && t[1] == '-') {
            const auto eq = t.find('=');
            if (eq != std::string::npos) {
                args.flags[t.substr(2, eq - 2)] = t.substr(eq + 1);
            } else if (i + 1 < tokens.size() && !(tokens[i + 1].size() > 1 && tokens[i + 1][0] == '-' && tokens[i + 1][1] == '-')) {
                args.flags[t.substr(2)] = tokens[++i];
            } else {
                args.flags[t.substr(2)] = "true";
            }
        } else {
            args.positional.push_back(t);
        }
    }
    return args;
}

CommandResult CommandRegistry::Execute(AppState& state, const std::string& line) {
    const auto tokens = Tokenize(line);
    if (tokens.empty()) return CommandResult::Ok();
    const CommandSpec* spec = Find(tokens[0]);
    if (!spec) {
        auto matches = Complete(tokens[0]);
        if (matches.size() == 1) spec = Find(matches[0]);   // unique prefix
        else if (!matches.empty())
            return CommandResult::Error(fmt::format("Ambiguous command '{}': {}", tokens[0], fmt::join(matches, ", ")));
        else
            return CommandResult::Error(fmt::format("Unknown command '{}'. Try `help`.", tokens[0]));
    }
    CommandArgs args = ParseArgs(std::vector<std::string>(tokens.begin() + 1, tokens.end()));
    try {
        return spec->handler(state, args);
    } catch (const std::exception& e) {
        return CommandResult::Error(fmt::format("{}: {}", spec->name, e.what()));
    }
}

CommandResult CommandRegistry::ExecuteScript(AppState& state, const std::string& script) {
    std::string combined;
    std::string current;
    for (char c : script) {
        if (c == ';' || c == '\n') {
            CommandResult r = Execute(state, current);
            if (!r.ok) return r;
            if (!r.message.empty()) combined += (combined.empty() ? "" : "\n") + r.message;
            current.clear();
        } else {
            current += c;
        }
    }
    CommandResult r = Execute(state, current);
    if (!r.ok) return r;
    if (!r.message.empty()) combined += (combined.empty() ? "" : "\n") + r.message;
    return CommandResult::Ok(combined);
}

std::vector<std::string> CommandRegistry::Complete(const std::string& prefix) const {
    std::vector<std::string> out;
    for (const auto& [name, spec] : commands)
        if (name.compare(0, prefix.size(), prefix) == 0) out.push_back(name);
    return out;
}

std::vector<const CommandSpec*> CommandRegistry::All() const {
    std::vector<const CommandSpec*> out;
    for (const auto& [name, spec] : commands) out.push_back(&spec);
    return out;
}

std::string CommandRegistry::HelpText(const std::string& name) const {
    if (!name.empty()) {
        const CommandSpec* spec = Find(name);
        if (!spec) return fmt::format("Unknown command '{}'", name);
        return fmt::format("{}\n  {}", spec->usage, spec->description);
    }
    // Group by category.
    std::map<std::string, std::vector<const CommandSpec*>> byCategory;
    for (const auto& [n, spec] : commands) byCategory[spec.category].push_back(&spec);
    std::string out;
    for (const auto& [category, specs] : byCategory) {
        out += fmt::format("[{}]\n", category);
        for (const CommandSpec* s : specs) out += fmt::format("  {:<34} {}\n", s->usage, s->description);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Built-in commands
// ---------------------------------------------------------------------------
namespace {

int ParseInt(const std::string& s) { return std::stoi(s); }
float ParseFloat(const std::string& s) { return std::stof(s); }

bool ParseBool(const std::string& s, bool& out) {
    std::string l = s;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    if (l == "on" || l == "true" || l == "1" || l == "yes") { out = true; return true; }
    if (l == "off" || l == "false" || l == "0" || l == "no") { out = false; return true; }
    return false;
}

// Parses "1 2 5-9 12" style atom lists (1-based in the UI, 0-based internally).
bool ParseAtomList(const std::vector<std::string>& tokens, size_t start, std::vector<int>& out) {
    for (size_t i = start; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        const auto dash = t.find('-', 1);
        try {
            if (dash != std::string::npos) {
                const int a = std::stoi(t.substr(0, dash)), b = std::stoi(t.substr(dash + 1));
                if (a > b) return false;
                for (int k = a; k <= b; ++k) out.push_back(k - 1);
            } else {
                out.push_back(std::stoi(t) - 1);
            }
        } catch (...) {
            return false;
        }
    }
    return !out.empty();
}

CommandResult ToggleFlag(bool& flag, const CommandArgs& args, const char* what) {
    if (args.size() == 0) flag = !flag;
    else if (!ParseBool(args[0], flag)) return CommandResult::Error(fmt::format("Expected on/off, got '{}'", args[0]));
    return CommandResult::Ok(fmt::format("{}: {}", what, flag ? "on" : "off"));
}

}  // namespace

void RegisterBuiltinCommands(CommandRegistry& r) {
    // ---- general ----
    r.Register({"help", "help [command]", "List commands, or describe one.", "general",
                [&r](AppState&, const CommandArgs& a) { return CommandResult::Ok(r.HelpText(a.size() ? a[0] : "")); }});
    r.Register({"echo", "echo <text...>", "Print text to the console.", "general",
                [](AppState&, const CommandArgs& a) { return CommandResult::Ok(fmt::format("{}", fmt::join(a.positional, " "))); }});
    r.Register({"clear", "clear", "Clear the console log.", "general",
                [](AppState& s, const CommandArgs&) { s.log.clear(); return CommandResult::Ok(); }});
    r.Register({"quit", "quit", "Exit ChemLab.", "general",
                [](AppState& s, const CommandArgs&) { s.quitRequested = true; return CommandResult::Ok("Bye"); }});
    r.Register({"debug", "debug [input on|off]", "Diagnostics: `debug input` overlays raylib vs ImGui mouse positions and DPI info.", "general",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() >= 1 && a[0] == "input") {
                        CommandArgs rest;
                        if (a.size() >= 2) rest.positional.push_back(a[1]);
                        return ToggleFlag(s.showInputDebug, rest, "Input debug overlay");
                    }
                    const Vector2 dpi = GetWindowScaleDPI();
                    const Vector2 m = GetMousePosition();
                    return CommandResult::Ok(fmt::format(
                        "screen {}x{}  render {}x{}  dpi {:.2f}x{:.2f}  highdpi {}\nraylib mouse ({:.0f}, {:.0f})\nuse `debug input on` for an on-screen overlay",
                        GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight(), dpi.x, dpi.y,
                        IsWindowState(FLAG_WINDOW_HIGHDPI) ? "on" : "off", m.x, m.y));
                }});
    r.Register({"status", "status", "Summarise what is loaded and shown.", "general", [](AppState& s, const CommandArgs&) {
                    const Structure* st = s.ActiveStructure();
                    if (!st) return CommandResult::Ok("No structure loaded");
                    const ChemicalData& a = st->frames.data[st->activeFrame];
                    return CommandResult::Ok(fmt::format(
                        "structure: {} ({}/{} structures)\nframe: {}/{}\natoms: {}  bonds: {}\nstyle: {}\nselected: {}\nmeasurements: {}",
                        st->name, s.activeStructure + 1, s.structures.size(), st->activeFrame + 1, st->frames.nframes, a.natoms,
                        a.BondCount(), RenderStyleName(s.render.style), s.selected.size(), s.measurements.size()));
                }});

    // ---- project ----
    r.Register({"project", "project <new dir [name]|open path|save|close|info>",
                "Create, open, save or close a project folder (chemlab.toml; its root becomes the working directory).", "project",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() == 0 || a[0] == "info") {
                        if (!s.project) return CommandResult::Ok("No project open (scratch session)");
                        const Project& p = *s.project;
                        std::string data;
                        for (const auto& d : p.DataDirs()) data += (data.empty() ? "" : ", ") + d.string();
                        std::string scripts;
                        for (const auto& f : p.config.scripts) scripts += (scripts.empty() ? "" : ", ") + f;
                        std::string out = fmt::format(
                            "project: {}\nroot (cwd): {}\nconfig: {}\nlayout: {}\nscene: {}{}\ndata: {}\nscenes: {}\ngraphs: {}\n"
                            "scripts: {}{}\noutput: {}\npython: {}\nstructures: {}{}",
                            p.config.name, p.Root().string(), p.ConfigPath().string(), p.LayoutPath().string(),
                            p.config.scene.active, p.config.scene.layout.empty() ? "" : " / " + p.config.scene.layout, data,
                            p.ScenesDir().string(), p.GraphsDir().string(), p.ScriptsDir().string(),
                            scripts.empty() ? "" : " [" + scripts + "]", p.OutputDir().string(), p.PythonExe(),
                            s.structures.size(), s.projectDirty ? "\n(unsaved changes)" : "");
                        return CommandResult::Ok(out);
                    }
                    if (a[0] == "new") return NewProject(s, a.size() > 1 ? a[1] : "", a.size() > 2 ? a[2] : "");
                    if (a[0] == "open") return OpenProject(s, a.size() > 1 ? a[1] : "");
                    if (a[0] == "save") return SaveProject(s);
                    if (a[0] == "close") return CloseProject(s);
                    return CommandResult::Error("Expected new, open, save, close or info");
                }});

    // ---- file ----
    r.Register({"load", "load <path.xyz>", "Load an xyz file as a new structure and make it active.", "file",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Error("usage: load <path.xyz>");
                    return LoadStructureFile(s, a[0], true);
                }});
    r.Register({"structure", "structure <n|list|remove n|rename n name>", "Switch, list, remove or rename loaded structures.", "file",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() == 0 || a[0] == "list") {
                        std::string out;
                        for (size_t i = 0; i < s.structures.size(); ++i)
                            out += fmt::format("{}{}. {} ({} frames)\n", (int)i == s.activeStructure ? "*" : " ", i + 1,
                                               s.structures[i].name, s.structures[i].frames.nframes);
                        return CommandResult::Ok(out.empty() ? "No structures loaded" : out);
                    }
                    if (a[0] == "remove" && a.size() >= 2) return RemoveStructure(s, ParseInt(a[1]) - 1);
                    if (a[0] == "rename" && a.size() >= 3) return RenameStructure(s, ParseInt(a[1]) - 1, a[2]);
                    return SetActiveStructure(s, ParseInt(a[0]) - 1);
                }});
    r.Register({"export", "export <path.xyz> [--all]", "Write the current frame (or all frames) as xyz.", "file",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Error("usage: export <path.xyz> [--all]");
                    return ExportXYZ(s, a[0], a.Has("all"));
                }});
    r.Register({"screenshot", "screenshot [path.png] [--size WxH] [--transparent]", "Render the view to a PNG.", "file",
                [](AppState& s, const CommandArgs& a) {
                    int w = s.exportSettings.screenshotWidth, h = s.exportSettings.screenshotHeight;
                    if (a.Has("size")) {
                        const std::string sz = a.Flag("size");
                        const auto x = sz.find('x');
                        if (x == std::string::npos) return CommandResult::Error("--size expects WxH, e.g. --size 1920x1080");
                        w = ParseInt(sz.substr(0, x)); h = ParseInt(sz.substr(x + 1));
                    }
                    return SaveScreenshot(s, a.size() ? a[0] : "", w, h, a.Has("transparent") || s.exportSettings.transparentBackground);
                }});
    r.Register({"watch", "watch [on|off]", "Reload files when they change on disk.", "file",
                [](AppState& s, const CommandArgs& a) { return ToggleFlag(s.watchFiles, a, "File watching"); }});

    // ---- frames ----
    r.Register({"frame", "frame <n|next|prev|first|last>", "Show a frame of the active structure.", "frames",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Ok(fmt::format("Frame {}/{}", s.ActiveFrameIndex() + 1, s.FrameCount()));
                    const std::string& w = a[0];
                    if (w == "next") return StepFrame(s, +1);
                    if (w == "prev" || w == "previous") return StepFrame(s, -1);
                    if (w == "first") return SetFrame(s, 0);
                    if (w == "last") return SetFrame(s, s.FrameCount() - 1);
                    return SetFrame(s, ParseInt(w) - 1);
                }});
    r.Register({"play", "play [fps] [--once]", "Play through the frames.", "frames", [](AppState& s, const CommandArgs& a) {
                    if (a.size() >= 1) s.playback.framesPerSecond = ParseFloat(a[0]);
                    if (a.Has("once")) s.playback.loop = false;
                    s.playback.playing = true;
                    s.playback.lastAdvance = GetTime();
                    return CommandResult::Ok(fmt::format("Playing at {:.1f} fps", s.playback.framesPerSecond));
                }});
    r.Register({"stop", "stop", "Stop playback and auto-rotation.", "frames", [](AppState& s, const CommandArgs&) {
                    s.playback.playing = false;
                    s.autoRotate = false;
                    return CommandResult::Ok("Stopped");
                }});

    // ---- view ----
    r.Register({"style", "style <ball-and-stick|spheres|sticks>", "Set the molecular render style.", "view",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Ok(RenderStyleName(s.render.style));
                    RenderStyle st;
                    if (!ParseRenderStyle(a[0].c_str(), st)) return CommandResult::Error("Unknown style '" + a[0] + "'");
                    s.render.style = st;
                    MarkGeometryChanged(s);
                    return CommandResult::Ok(fmt::format("Style: {}", RenderStyleName(st)));
                }});
    r.Register({"set", "set <ball_scale|stick_radius|sphere_scale|rotate_speed|bond_tolerance> <value>",
                "Set a numeric rendering/calculation parameter.", "view", [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 2) return CommandResult::Error("usage: set <name> <value>");
                    const float v = ParseFloat(a[1]);
                    if (a[0] == "ball_scale") s.render.ballScale = v;
                    else if (a[0] == "stick_radius") s.render.stickRadius = v;
                    else if (a[0] == "sphere_scale") s.render.sphereScale = v;
                    else if (a[0] == "rotate_speed") { s.autoRotateDegPerSec = v; return CommandResult::Ok(); }
                    else if (a[0] == "bond_tolerance") { s.calc.bondTolerance = v; return RecomputeBonds(s); }
                    else return CommandResult::Error("Unknown parameter '" + a[0] + "'");
                    MarkGeometryChanged(s);
                    return CommandResult::Ok(fmt::format("{} = {}", a[0], v));
                }});
    r.Register({"grid", "grid [on|off]", "Toggle the ground grid.", "view",
                [](AppState& s, const CommandArgs& a) { return ToggleFlag(s.drawGrid, a, "Grid"); }});
    r.Register({"numbers", "numbers [on|off]", "Toggle atom index labels.", "view",
                [](AppState& s, const CommandArgs& a) { return ToggleFlag(s.drawAtomNumbers, a, "Atom numbers"); }});
    r.Register({"rotate", "rotate [on|off] [deg/s]", "Spin the molecule about the vertical axis.", "view",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() >= 2) s.autoRotateDegPerSec = ParseFloat(a[1]);
                    return ToggleFlag(s.autoRotate, a, "Auto-rotate");
                }});
    r.Register({"background", "background <color>", "Set the 3D background colour (name, #rrggbb or r g b).", "view",
                [](AppState& s, const CommandArgs& a) {
                    Color c;
                    if (!ParseColor(a.positional, 0, c)) return CommandResult::Error("Could not parse colour");
                    s.background = c;
                    return CommandResult::Ok("Background set");
                }});
    r.Register({"camera", "camera <reset|x|y|z|-x|-y|-z>", "Reset the camera or look down an axis.", "view",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1 || a[0] == "reset") { ResetCamera(s); return CommandResult::Ok("Camera reset"); }
                    const std::string& w = a[0];
                    const bool flip = w[0] == '-';
                    const char axis = flip ? w[1] : w[0];
                    if (axis != 'x' && axis != 'y' && axis != 'z') return CommandResult::Error("Expected reset, x, y, z, -x, -y or -z");
                    LookDownAxis(s, axis - 'x', flip);
                    return CommandResult::Ok(fmt::format("Looking down {}", w));
                }});
    r.Register({"panel", "panel <name> [show|hide|toggle]", "Show or hide a panel (controls, structure, plot, calculate, output, workflows, export, active, console, graph, canvas).",
                "view", [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Error("usage: panel <name> [show|hide|toggle]");
                    bool* flag = nullptr;
                    const std::string& n = a[0];
                    // Aliases kept from the pre-registry command names.
                    if (n == "controls") flag = &s.PanelOpen("controls");
                    else if (n == "structure" || n == "view" || n == "structure_view") flag = &s.PanelOpen("structure_view");
                    else if (n == "plot" || n == "plot_2d" || n == "2d") flag = &s.PanelOpen("plot_2d");
                    else if (n == "calculate") flag = &s.PanelOpen("calculate");
                    else if (n == "output") flag = &s.PanelOpen("output");
                    else if (n == "export") flag = &s.PanelOpen("export");
                    else if (n == "active" || n == "active_structure") flag = &s.PanelOpen("active_structure");
                    else if (n == "console") flag = &s.PanelOpen("console");
                    else if (n == "graph" || n == "nodes" || n == "node_graph") flag = &s.PanelOpen("node_graph");
                    else if (n == "canvas" || n == "graph_canvas") flag = &s.PanelOpen("graph_canvas");
                    else if (n == "workflows") flag = &s.PanelOpen("workflows");
                    if (!flag) return CommandResult::Error("Unknown panel '" + n + "'");
                    if (a.size() < 2 || a[1] == "toggle") *flag = !*flag;
                    else if (a[1] == "show") *flag = true;
                    else if (a[1] == "hide") *flag = false;
                    else return CommandResult::Error("Expected show, hide or toggle");
                    return CommandResult::Ok(fmt::format("Panel {}: {}", n, *flag ? "shown" : "hidden"));
                }});
    r.Register({"theme", "theme <mocha|nord|darcula>", "Switch the UI colour theme.", "view", [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Ok(UIThemeName(s.theme));
                    UITheme t;
                    if (!ParseUITheme(a[0].c_str(), t)) return CommandResult::Error("Unknown theme (mocha, nord, darcula)");
                    s.theme = t;
                    ApplyChemLabTheme(t);
                    return CommandResult::Ok(fmt::format("Theme: {}", UIThemeName(t)));
                }});
    r.Register({"layout", "layout reset", "Restore the default dock layout.", "view", [](AppState& s, const CommandArgs&) {
                    s.resetLayoutRequested = true;
                    return CommandResult::Ok("Layout reset");
                }});
    r.Register({"ui", "ui [list|builder|<name>]", "Alias of `scene`: list, show or build scenes.", "view",
                [](AppState& s, const CommandArgs& a) {
                    std::string line = "scene";
                    for (size_t i = 0; i < a.size(); ++i) line += " \"" + a[i] + "\"";
                    return s.commands.Execute(s, line);
                }});
    r.Register({"plot", "plot <energy|measurements|name> | plot list | plot remove <name> | plot clear",
                "Choose the plot shown in the 2D Plot panel (built-in or published by name), or manage published plots.",
                "view", [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1 || a[0] == "list") {
                        std::string out = "Plots:";
                        for (const char* b : {"energy", "measurements"})
                            out += fmt::format("\n  {}{}", b, s.SelectedPlotName() == b ? "  (shown)" : "");
                        for (const auto& p : s.plots)
                            out += fmt::format("\n  {}  [{} series]{}", p.name, p.spec.series.size(),
                                               s.SelectedPlotName() == p.name ? "  (shown)" : "");
                        return CommandResult::Ok(out);
                    }
                    if (a[0] == "remove") {
                        if (a.size() < 2) return CommandResult::Error("usage: plot remove <name>");
                        if (!s.RemovePlot(a[1])) return CommandResult::Error(fmt::format("No plot named '{}'", a[1]));
                        return CommandResult::Ok("Removed plot " + a[1]);
                    }
                    if (a[0] == "clear") {
                        s.ClearPlots();
                        return CommandResult::Ok("Published plots cleared");
                    }
                    if (!s.SelectPlot(a[0]))
                        return CommandResult::Error(fmt::format("No plot named '{}' (try `plot list`)", a[0]));
                    s.PanelOpen("plot_2d") = true;
                    return CommandResult::Ok("Plot: " + a[0]);
                }});

    // ---- selection ----
    r.Register({"select", "select <atoms...|all|none|invert|element X> [--add]",
                "Select atoms by 1-based index/range (e.g. 1 3 5-9), element, or all/none/invert.", "selection",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Ok(fmt::format("{} atoms selected", s.selected.size()));
                    const bool add = a.Has("add");
                    if (a[0] == "all") return SelectAll(s);
                    if (a[0] == "none") return SelectNone(s);
                    if (a[0] == "invert") return InvertSelection(s);
                    if (a[0] == "element" && a.size() >= 2) return SelectByElement(s, a[1], add);
                    std::vector<int> atoms;
                    if (!ParseAtomList(a.positional, 0, atoms)) return CommandResult::Error("Could not parse atom list");
                    return SelectAtoms(s, std::set<int>(atoms.begin(), atoms.end()), add);
                }});
    r.Register({"color", "color <name|#rrggbb|r g b> | color reset", "Colour the selected atoms.", "selection",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() >= 1 && a[0] == "reset") return ResetColors(s);
                    Color c;
                    if (!ParseColor(a.positional, 0, c)) return CommandResult::Error("Could not parse colour (try `color red`, `color #ff8800`, `color 255 136 0`)");
                    return ColorSelection(s, c);
                }});
    r.Register({"alpha", "alpha <0..1>", "Set the opacity of the selected atoms.", "selection", [](AppState& s, const CommandArgs& a) {
                    if (a.size() < 1) return CommandResult::Error("usage: alpha <0..1>");
                    return SetSelectionAlpha(s, ParseFloat(a[0]));
                }});

    // ---- measurements ----
    r.Register({"measure", "measure <i j [k [l]]> | measure clear | measure list", "Add a distance/angle/dihedral between atoms (1-based).",
                "measure", [](AppState& s, const CommandArgs& a) {
                    if (a.size() >= 1 && a[0] == "clear") return ClearMeasurements(s);
                    if (a.size() == 0 || a[0] == "list") {
                        const ChemicalData* atoms = s.ActiveChem();
                        if (!atoms) return CommandResult::Error("No structure loaded");
                        std::string out;
                        for (const Measurement& m : s.measurements)
                            out += fmt::format("{} = {:.4f}\n", MeasurementLabel(m), MeasurementValue(*atoms, m));
                        return CommandResult::Ok(out.empty() ? "No measurements" : out);
                    }
                    std::vector<int> atoms;
                    if (!ParseAtomList(a.positional, 0, atoms)) return CommandResult::Error("Could not parse atom indices");
                    return AddMeasurement(s, atoms);
                }});

    // ---- calculate ----
    r.Register({"bonds", "bonds [tolerance]", "Recompute covalent bonds from covalent radii.", "calculate",
                [](AppState& s, const CommandArgs& a) {
                    if (a.size() >= 1) s.calc.bondTolerance = ParseFloat(a[0]);
                    return RecomputeBonds(s);
                }});

    graph::RegisterGraphCommands(r);
    graph::RegisterWorkflowCommands(r);
}
