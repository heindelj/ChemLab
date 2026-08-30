// ChemLab entry point.
//
//   raylib   owns the window, the GL context and all 3D rendering (into an
//            off-screen texture per viewport).
//   ImGui    (docking branch, from imgui_bundle) + ImPlot draw every panel,
//            2D plot and the command bar, through the rlImGui backend.
//
// Usage: ChemLab [file.xyz | project_dir | chemlab.toml ...] [--run "<command>; <command>"] [--snap out.png] [--exit]

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "raylib.h"

#include "app/actions.h"
#include "app/app_state.h"
#include "ui/ui.h"

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_WARNING);

    std::vector<std::string> files;
    std::string startupScript;
    std::string snapshotPath;    // --snap <png>: grab the whole window after a few frames, then quit
    bool headlessExit = false;   // --exit: quit after a few frames (used for smoke tests)
    int benchFrames = 0;         // --bench N: run N frames, print timing stats, exit
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--run") == 0 && i + 1 < argc) startupScript = argv[++i];
        else if (std::strcmp(argv[i], "--snap") == 0 && i + 1 < argc) snapshotPath = argv[++i];
        else if (std::strcmp(argv[i], "--exit") == 0) headlessExit = true;
        else if (std::strcmp(argv[i], "--bench") == 0 && i + 1 < argc) benchFrames = std::atoi(argv[++i]);
        else files.emplace_back(argv[i]);
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    InitWindow(1760, 1100, "ChemLab");
    SetWindowMinSize(800, 500);
    {
        // Fit the window to the monitor it opened on. If the requested size does
        // not fit, macOS shrinks the window *before* raylib installs its resize
        // callback, so raylib keeps believing the window is 1760x1100 and every
        // mouse position/projection is off until the user resizes by hand.
        // Setting a size that differs from the requested one forces the resize
        // event and re-syncs raylib with the real window.
        const int monitor = GetCurrentMonitor();
        const int mw = GetMonitorWidth(monitor), mh = GetMonitorHeight(monitor);
        int w = 1760, h = 1100;
        if (mw > 0 && mh > 0) {
            w = std::min(w, mw - 80);
            h = std::min(h, mh - 120);   // menu bar + title bar + dock
        }
        if (w != 1760 || h != 1100) SetWindowSize(std::max(w, 800), std::max(h, 500));
        else SetWindowSize(w - 1, h - 1);   // still force the event; one pixel is invisible
    }
    if (benchFrames <= 0) SetTargetFPS(60);   // uncapped while benchmarking
    SetExitKey(KEY_NULL);   // Escape is used by the UI

    AppState state;
    state.viewport.Init();
    UIInit(state);

    std::string projectPath;
    for (const std::string& f : files) {
        if (Project::LooksLikeProject(f)) projectPath = f;
        else LoadStructureFile(state, f, true);
    }
    if (!projectPath.empty()) RunCommandLine(state, fmt::format("project open \"{}\"", projectPath));
    if (files.empty()) {
        // Convenience for development: show something when launched bare.
        const std::string sample = std::string(ASSETS_PATH) + "caffeine.xyz";
        if (FileExists(sample.c_str())) LoadStructureFile(state, sample, true);
    }
    if (!startupScript.empty()) RunCommandLine(state, startupScript);

    int framesDrawn = 0;
    while (!WindowShouldClose() && !state.quitRequested) {
        // Dropped files become new structures.
        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            for (unsigned int i = 0; i < dropped.count; ++i) LoadStructureFile(state, dropped.paths[i], true);
            UnloadDroppedFiles(dropped);
        }

        UpdateFileWatch(state);
        UpdatePlayback(state);
        {
            static std::string lastTitle;
            const std::string title = state.project ? fmt::format("ChemLab - {}{}", state.project->config.name, state.projectDirty ? " *" : "")
                                                    : std::string("ChemLab");
            if (title != lastTitle) { SetWindowTitle(title.c_str()); lastTitle = title; }
        }

        BeginDrawing();
        FixAppleScreenScale();
        ClearBackground(Color{30, 31, 34, 255});
        UIFrame(state);
        EndDrawing();

        ++framesDrawn;
        if (!snapshotPath.empty() && framesDrawn == 6) {
            Image shot = LoadImageFromScreen();
            ExportImage(shot, snapshotPath.c_str());
            UnloadImage(shot);
            break;
        }
        if (headlessExit && framesDrawn >= 3) break;
        if (benchFrames > 0 && framesDrawn >= benchFrames) {
            const double elapsed = GetTime();
            printf("bench: %d frames in %.2f s -> %.2f ms/frame (%.0f fps)\n", framesDrawn, elapsed,
                   1000.0 * elapsed / framesDrawn, framesDrawn / elapsed);
            break;
        }
    }

    UIShutdown(state);
    state.model.Unload();
    state.viewport.Shutdown();
    CloseWindow();
    return 0;
}
