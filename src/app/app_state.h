#pragma once
// All mutable application state in one place. The UI panels read and write
// it directly; anything that has side effects on the scene goes through the
// functions in actions.h so the command bar and the widgets stay in sync.

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "raylib.h"

#include "app/commands.h"
#include "app/project.h"
#include "core/molecule.h"
#include "plot/plot_spec.h"
#include "render/molecular_model.h"
#include "render/viewport.h"
#include "ui/theme.h"
#include "ui/ui_spec.h"

namespace graph {
struct GraphSystem;   // node graph + generated-data store (src/graph), pimpl-style
}

struct Structure {
    std::string name;
    std::string path;      // empty for structures created in-app
    Frames frames;
    int activeFrame = 0;
};

struct Measurement {
    std::array<int, 4> atoms{-1, -1, -1, -1};
    int count = 0;   // 2 = distance, 3 = angle, 4 = dihedral
};

enum class LogLevel { Info, Warning, Error, Command, Result };

struct LogEntry {
    LogLevel level;
    std::string text;
    double time;
};

struct PlaybackState {
    bool playing = false;
    bool loop = true;
    float framesPerSecond = 10.0f;
    double lastAdvance = 0.0;
};

struct ExportSettings {
    int screenshotWidth = 1600;
    int screenshotHeight = 1600;
    bool transparentBackground = false;
    std::string outputDirectory;  // empty = ask / cwd
    std::string lastScreenshotPath;
    std::string lastXYZPath;
};

struct CalculationSettings {
    float bondTolerance = 0.4f;   // angstrom slack on covalent radii sum
};

struct AppState {
    AppState();
    ~AppState();
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;

    // ---- project ----
    std::optional<Project> project;   // none = scratch session
    std::string iniFileName = "chemlab_imgui.ini";   // backing store for io.IniFilename
    std::string pendingIniFile;       // switch ImGui settings to this file before the next frame
    bool projectDirty = false;

    // ---- data ----
    std::vector<Structure> structures;
    int activeStructure = -1;

    // ---- rendering ----
    Viewport viewport;
    MolecularModel model;         // GPU model of the active frame
    RenderSettings render;
    Color background = Color{30, 30, 30, 255};
    bool drawGrid = true;
    bool drawAtomNumbers = false;
    bool drawMeasurements = true;
    bool autoRotate = false;
    float autoRotateDegPerSec = 20.0f;
    bool watchFiles = true;
    bool modelDirty = true;       // rebuild the GPU model before the next draw

    // ---- selection / measurements ----
    std::set<int> selected;
    std::array<int, 4> pendingMeasurement{-1, -1, -1, -1};
    int pendingCount = 0;
    std::vector<Measurement> measurements;
    // Bumped on every measurement mutation; keys the measurement-plot cache.
    uint64_t measurementsVersion = 0;
    int hoveredAtom = -1;
    float pickerColor[4] = {0.2f, 0.5f, 1.0f, 1.0f};

    // ---- playback ----
    PlaybackState playback;

    // ---- calculations / export ----
    CalculationSettings calc;
    ExportSettings exportSettings;

    // ---- node graph ----
    // Everything node-graph related (graph, evaluation, generated-data store)
    // lives behind this pointer in src/graph so its internals can change
    // without touching the rest of the app.
    std::unique_ptr<graph::GraphSystem> graphSystem;
    graph::GraphSystem& GraphSys();

    // ---- ui ----
    // Per-frame series for the measurements plot, rebuilt only when the
    // measurements or the underlying frames change (evaluating every
    // measurement over 15k frames each rendered frame is far too slow).
    struct MeasurementPlotCache {
        int structureIndex = -1;
        uint64_t measurementsVersion = ~0ull;
        uint64_t dataVersion = ~0ull;
        std::vector<double> x;
        std::vector<std::vector<double>> series;   // parallel to measurements
    } measurementPlotCache;

    // ---- 2D plots ----
    // The 2D Plot panel shows one of: the built-in per-frame plots (energy,
    // measurements) or a plot published by name (by Plot nodes, scripts, ...).
    // twoDPlotIndex < kBuiltinPlotCount picks a built-in plot; otherwise it is
    // kBuiltinPlotCount + index into `plots`.
    static constexpr int kBuiltinPlotCount = 2;
    std::vector<plot::NamedPlot> plots;
    int twoDPlotIndex = 0;
    // Replace (or add) the plot called `name`. A brand-new name becomes the
    // selected plot when `activate` is set.
    plot::NamedPlot& PublishPlot(const std::string& name, plot::PlotSpec spec, bool activate = true);
    plot::NamedPlot* FindPlot(const std::string& name);
    bool RemovePlot(const std::string& name);   // keeps the selection sensible
    void ClearPlots();
    bool SelectPlot(const std::string& name);   // "energy", "measurements" or a published name
    std::string SelectedPlotName() const;
    bool showImGuiDemo = false;
    bool showImPlotDemo = false;
    bool showMetrics = false;
    bool showInputDebug = false;
    UITheme theme = UITheme::Mocha;   // crosshairs + numbers for diagnosing mouse/DPI mismatches
    bool resetLayoutRequested = false;   // re-dock and reset panel visibility from the active UI
    bool quitRequested = false;

    // ---- UI system ----
    // The current arrangement is one UIDefinition among several: a layout
    // (how the dockspace splits into slots) plus panels assigned to slots.
    // See ui/ui_spec.h, ui/panel_registry.h and ui/ui_builder.h.
    std::vector<UIDefinition> uis;           // built-in + user-defined UIs
    int activeUI = 0;                        // index into `uis`
    std::map<std::string, bool> panelOpen;   // panel id -> window currently shown
    std::map<std::string, bool> graphViewOpen;   // panel id -> its "Graph: <panel>" window is shown
    UIBuilderState uiBuilder;
    bool& PanelOpen(const char* id);

    // ---- command bar ----
    CommandRegistry commands;
    std::deque<LogEntry> log;
    std::vector<std::string> commandHistory;
    std::string commandInput;
    bool focusCommandBar = false;
    std::string lastCommandResult;
    bool lastCommandOk = true;

    // ---- helpers ----
    Structure* ActiveStructure();
    const Structure* ActiveStructure() const;
    Frames* ActiveFrames();
    Atoms* ActiveAtoms();
    const Atoms* ActiveAtoms() const;
    int ActiveFrameIndex() const;
    int FrameCount() const;
};
