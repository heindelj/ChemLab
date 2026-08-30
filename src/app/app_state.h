#pragma once
// All mutable application state in one place. The UI panels read and write
// it directly; anything that has side effects on the scene goes through the
// functions in actions.h so the command bar and the widgets stay in sync.

#include <array>
#include <optional>
#include <deque>
#include <set>
#include <string>
#include <vector>

#include "raylib.h"

#include "app/commands.h"
#include "app/project.h"
#include "core/molecule.h"
#include "render/molecular_model.h"
#include "render/viewport.h"
#include "ui/theme.h"

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

    float twoDPaneFraction = 0.30f;
    int twoDPlotIndex = 0;
    bool showImGuiDemo = false;
    bool showImPlotDemo = false;
    bool showMetrics = false;
    bool showInputDebug = false;
    UITheme theme = UITheme::Mocha;   // crosshairs + numbers for diagnosing mouse/DPI mismatches
    bool resetLayoutRequested = false;
    bool quitRequested = false;
    struct PanelVisibility {
        bool controls = true, structureView = true, calculate = true,
             output = true, exportPanel = true, activeStructure = true, console = false;
    } panels;

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
