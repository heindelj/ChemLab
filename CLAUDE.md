# ChemLab — notes for Claude

Interactive computational-chemistry front end: raylib (3D), Dear ImGui docking +
ImPlot (UI), imgui-node-editor (graphs). C++20, CMake, deps via FetchContent.
See README.md for the user-facing layout, commands and the graph design.

## Building

Default build type is **RelWithDebInfo with asserts on** (CMakeLists strips
`-DNDEBUG`), so `IM_ASSERT`s in ImGui / the node editor / ImPlot fire instead of
turning into silent memory corruption. `-DCMAKE_BUILD_TYPE=Release` disables them.

```
cmake -B build && cmake --build build -j8
./build/ChemLab assets/caffeine.xyz
```

`-DCHEMLAB_ASAN=ON` adds AddressSanitizer to the whole build.

## Headless testing — how Claude runs the app

Joe works on macOS; Claude's sandbox is x86-64 Linux with no GPU. The app runs
there under Xvfb with Mesa software GL, which is enough to catch crashes,
asserts, ASan reports and to take screenshots of the UI. Loop:

1. **Bundle the source on the Mac** (the `_sync/` folder is git-ignored):
   `tar czf _sync/chemlab_src.tgz --exclude='assets/stress_water_103k.xyz' CMakeLists.txt README.md CLAUDE.md src assets web`
   then stage `_sync/chemlab_src.tgz` into the sandbox and `tar xzf` it into
   `~/chemlab`. Re-bundle after every batch of edits: the sandbox works from a
   snapshot, not the live tree.
2. **First-time sandbox setup** (once per sandbox):
   `apt-get install -y libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libgl-dev libglu1-mesa-dev libxkbcommon-dev`
   (xvfb, cmake, g++ and Mesa are preinstalled). Then
   `cmake -B build -DCHEMLAB_ASAN=ON` and `cmake --build build -j$(nproc)`.
   The first build fetches every dependency and takes ~20 min — run it with
   `nohup ... &` and poll `build.log`; rebuilds are incremental (~1–2 min).
3. **Run headless**:
   ```
   export LIBGL_ALWAYS_SOFTWARE=1 ASAN_OPTIONS=detect_leaks=0
   xvfb-run -s "-screen 0 1600x1000x24" ./build/ChemLab assets/caffeine.xyz \
       --run "graph demo; graph run; graph new t; canvas add core.text 100 100" --exit
   ```
   `--run "a; b; c"` executes command-bar commands at startup and echoes each
   result to stdout — everything in the UI is reachable this way (`help` lists
   commands). `--exit` quits after 3 frames. `--snap out.png` grabs the whole
   window at frame 6 and quits (do **not** combine with `--exit`, which wins
   first); view the PNG with the Read tool to check layout, colours, etc.
   ASan output and raylib `INFO:` lines go to stderr — filter with
   `grep -v "^INFO\|^WARNING"`.
4. Fix, re-bundle, rebuild, rerun. Apply the fix to the Mac tree too (the
   sandbox copy is throwaway).

Caveats: Metal / macOS-only issues, Retina scaling and GPU-driver behaviour do
not reproduce in the sandbox; anything touching those still needs a run on the
Mac. `lldb -- ./build/ChemLab` then `bt` gives a usable backtrace there.

## Conventions worth knowing

- Every panel is a graph underneath (`GraphSystem::panelGraphs`); the Node
  Graph and Graph Canvas panels are free-form graphs. Nodes have a `kind`
  (build / simulate / analyze / visualize / other) that colours them.
- Visualize nodes (Render 3D, Plot 2D) feed their panel when they sit in that
  panel's graph, and open their own dockable window otherwise
  (`GraphSystem::nodeViews`, drawn by `ui/node_views.cpp`).
- Named graphs from the Graph Canvas are saved as `graphs/<name>.json`
  (`graph new/save/load/list`); `graph_io.cpp` is the format.
- `IM_ASSERT` is on in the default build; keep it that way when testing.
- Another Claude session may be editing the same tree; check `git status` /
  file mtimes before rewriting a file wholesale, and prefer targeted edits.
