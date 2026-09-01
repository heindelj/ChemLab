// ChemLab entry point.
//
//   raylib   owns the window, the GL context and all 3D rendering (into an
//            off-screen texture per viewport).
//   ImGui    (docking branch, from imgui_bundle) + ImPlot draw every panel,
//            2D plot and the command bar, through the rlImGui backend.
//
// Usage: ChemLab [file.xyz | project_dir | chemlab.toml ...] [--run "<command>; <command>"] [--snap out.png] [--exit]
//
// Web build: the frame body lives in RunFrame() so the browser can drive it
// through emscripten_set_main_loop_arg(). raylib's WindowShouldClose() calls
// emscripten_sleep() there, which would need ASYNCIFY, so the web build never
// calls it.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "raylib.h"

#if defined(__EMSCRIPTEN__)
#include <cstdarg>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include "app/actions.h"
#include "app/app_state.h"
#include "ui/ui.h"

namespace {

#if defined(__EMSCRIPTEN__)
// raylib's web backend has no GetWindowScaleDPI(); its stub logs a warning on
// every call, and rlImGui calls it once per frame. Drop just that message so
// real warnings (shader compile failures in particular) stay visible.
void WebTraceLog(int logLevel, const char* text, va_list args) {
    if (logLevel == LOG_WARNING && std::strstr(text, "GetWindowScaleDPI") != nullptr) return;
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), text, args);
    printf("%s\n", buffer);
}
#endif

struct FrameContext {
    AppState* state = nullptr;
    std::string snapshotPath;   // --snap <png>: grab the whole window after a few frames, then quit
    bool headlessExit = false;  // --exit: quit after a few frames (used for smoke tests)
    int benchFrames = 0;        // --bench N: run N frames, print timing stats, exit
    int framesDrawn = 0;
    bool done = false;          // set by --snap/--exit/--bench
};

void RunFrame(void* userData) {
    FrameContext& ctx = *static_cast<FrameContext*>(userData);
    AppState& state = *ctx.state;

    // Dropped files become new structures. On the web these are files the user
    // dragged onto the canvas; Emscripten's GLFW writes them into the virtual
    // filesystem first, so the path raylib reports is readable as usual.
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

    ++ctx.framesDrawn;
    if (!ctx.snapshotPath.empty() && ctx.framesDrawn == 6) {
        Image shot = LoadImageFromScreen();
        ExportImage(shot, ctx.snapshotPath.c_str());
        UnloadImage(shot);
        ctx.done = true;
    }
    if (ctx.headlessExit && ctx.framesDrawn >= 3) ctx.done = true;
    if (ctx.benchFrames > 0 && ctx.framesDrawn >= ctx.benchFrames) {
        const double elapsed = GetTime();
        printf("bench: %d frames in %.2f s -> %.2f ms/frame (%.0f fps)\n", ctx.framesDrawn, elapsed,
               1000.0 * elapsed / ctx.framesDrawn, ctx.framesDrawn / elapsed);
        ctx.done = true;
    }
}

FrameContext gFrame;

}  // namespace

int main(int argc, char** argv) {
#if defined(__EMSCRIPTEN__)
    SetTraceLogCallback(WebTraceLog);
#endif
    SetTraceLogLevel(LOG_WARNING);

    std::vector<std::string> files;
    std::string startupScript;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--run") == 0 && i + 1 < argc) startupScript = argv[++i];
        else if (std::strcmp(argv[i], "--snap") == 0 && i + 1 < argc) gFrame.snapshotPath = argv[++i];
        else if (std::strcmp(argv[i], "--exit") == 0) gFrame.headlessExit = true;
        else if (std::strcmp(argv[i], "--bench") == 0 && i + 1 < argc) gFrame.benchFrames = std::atoi(argv[++i]);
        else files.emplace_back(argv[i]);
    }

#if defined(__EMSCRIPTEN__)
    // The canvas is sized by the page and kept in sync by raylib's own resize
    // callback (FLAG_WINDOW_RESIZABLE). HIGHDPI is left off so ImGui's mouse
    // coordinates and the framebuffer stay in the same space.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    {
        // GLFW-on-Emscripten refuses a 0x0 window, so seed it from the canvas
        // element. raylib's own resize callback then keeps it matched to the
        // browser window from the first frame on (FLAG_WINDOW_RESIZABLE).
        int cw = 0, ch = 0;
        emscripten_get_canvas_element_size("#canvas", &cw, &ch);
        InitWindow(cw > 0 ? cw : 1280, ch > 0 ? ch : 800, "ChemLab");
    }
    SetWindowMinSize(320, 240);
#else
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
    if (gFrame.benchFrames <= 0) SetTargetFPS(60);   // uncapped while benchmarking
#endif
    SetExitKey(KEY_NULL);   // Escape is used by the UI

    AppState state;
    gFrame.state = &state;
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
        // On the web this is the preloaded copy under assets/.
        const std::string sample = std::string(ASSETS_PATH) + "caffeine.xyz";
        if (FileExists(sample.c_str())) LoadStructureFile(state, sample, true);
    }
    if (!startupScript.empty()) {
        RunCommandLine(state, startupScript);
        // Echo the result so --run is usable headlessly (smoke tests, CI).
        if (!state.lastCommandResult.empty())
            printf("%s%s\n", state.lastCommandOk ? "" : "error: ", state.lastCommandResult.c_str());
    }

#if defined(__EMSCRIPTEN__)
    // 0 fps = requestAnimationFrame; 1 = never return (the browser owns the loop).
    emscripten_set_main_loop_arg(RunFrame, &gFrame, 0, 1);
    return 0;   // not reached
#else
    while (!WindowShouldClose() && !state.quitRequested && !gFrame.done) RunFrame(&gFrame);

    UIShutdown(state);
    state.model.Unload();
    state.viewport.Shutdown();
    CloseWindow();
    return 0;
#endif
}
