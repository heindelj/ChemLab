# ChemLab

ChemLab is an interactive front end for computational chemistry, built on:

- **raylib** for the window, GL context and all 3D rendering (molecules are
  drawn into an off-screen texture per viewport), and
- **Dear ImGui** (docking branch) + **ImPlot** for every panel, 2D plot and the
  command bar. ImGui/ImPlot/ImPlot3D are pulled from the
  [imgui_bundle](https://github.com/pthom/imgui_bundle) forks at the exact
  commits imgui_bundle v1.92.900 pins, so the C++ API matches the Python
  `imgui_bundle` one-to-one. [rlImGui](https://github.com/raylib-extras/rlImGui)
  is the backend that glues the two together.

Today it is a molecular viewer with a docked workspace, measurements, trajectory
playback and a scriptable command bar; the layout is the one the calculation
workflows will grow into.

## Layout

```
+----------------+---------------------------------+---------------------+
| Controls       | Structure View (3D, raylib)     | Active Structure    |
|  frames        |                                 |  loaded files       |
|  rendering     |                                 +---------------------+
|  selection     +---------------------------------+ Calculate | Output  |
|  measurements  | 2D Plot (ImPlot):               |  per-frame table    |
+----------------+  energy / measurements vs frame |                     |
| Export         |                                 |                     |
+----------------+---------------------------------+---------------------+
| > command bar                                     status     [Console] |
+------------------------------------------------------------------------+
```

The 3D view and the 2D plot are two independent panels, so the divider
between them is an ordinary dock splitter: either can be resized, moved,
tabbed with another panel, torn off or closed on its own. Panels can be
dragged, tabbed and closed (View menu / `panel` command);
`View > Reset layout` restores the default. A layout saved by an older
build gets the 2D Plot panel docked under the 3D view automatically.

### 3D view

- drag: rotate, right/middle drag: pan, wheel: zoom, double right-click or `R`: reset
- click atoms: distance (2), angle (3), dihedral (4). `Enter` keeps a partial
  selection, `Esc` cancels. Measurements are tracked across every frame in the
  Calculation Output table and the "Measurements per frame" plot.
- shift-click: select atoms (then colour them from the Controls panel)
- `Left`/`Right`: step frames, `Space`: play/pause, `G`: grid, `N`: atom numbers
- files can be dropped onto the window

### Command bar

`Ctrl+K` focuses the bar at the bottom of the window. `Tab` completes command
names, `Up`/`Down` walk the history, `;` separates commands, and `help` lists
everything. Every button in the UI calls the same functions the commands do,
so the whole application is scriptable from here (and, later, by an agent):

```
load water.xyz
frame 20; measure 1 4; measure 1 4 7
select element O; color #ff8800; alpha 0.6
style spheres; set sphere_scale 0.8
plot measurements
screenshot out.png --size 1920x1080 --transparent
export traj.xyz --all
```

Commands live in `src/app/commands.cpp`; the `CommandRegistry` is the surface
new features should register themselves with. A startup script can be passed
on the command line:

```sh
./build/ChemLab traj.xyz --run "measure 1 2; plot measurements"
```

`--snap out.png` renders a few frames, saves the whole window and exits
(used for headless smoke tests), and `--exit` just quits after a few frames.

## Projects

A project is a folder with a `chemlab.toml` at its root; everything it
references is stored relative to that folder so it can be moved, zipped or
committed as a unit:

```
my_project/
  chemlab.toml     # name, view settings, structures (+ frame, measurements), startup commands
  layout.ini       # the dock layout, written by ChemLab for this project
  data/traj.xyz    # referenced as "data/traj.xyz"
```

`File > New project...` adopts whatever is loaded into a new folder,
`File > Open project...` (or `./build/ChemLab my_project`) restores it, and
`Ctrl+Shift+S` saves. The same is available as `project new|open|save|close|info`
on the command bar. The file is meant to be hand-edited; for example:

```toml
[project]
name = "Water cages"

[view]
style = "sticks"
background = "#202030"

[[structures]]
path = "data/w20.xyz"
frame = 42
measurements = [[1, 4], [1, 4, 7]]

[startup]
commands = ["plot measurements", "rotate on 15"]
```

`[scripts] files = [...]` is reserved for per-project analysis scripts.

## Source layout

```
src/core     molecule data, xyz IO, geometry, element tables (no rendering)
src/render   lighting shader, GPU molecular model, orbit camera, off-screen viewport
src/app      AppState, actions (every state change), the command registry, projects (chemlab.toml)
src/ui       ImGui: theme, dock layout, panels, command bar
```

## Building

Requirements: CMake 3.18+, a C++20 compiler, git (dependencies are fetched on
first configure). On macOS install the Xcode Command Line Tools
(`xcode-select --install`) and CMake (`brew install cmake`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ChemLab assets/caffeine.xyz
```

On Linux the usual raylib X11/GL development packages are needed
(`libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev`).
File dialogs use `portable-file-dialogs` (osascript on macOS, zenity/kdialog on Linux).

If Homebrew's `cmake` is not on your PATH: `export PATH="/opt/homebrew/bin:$PATH"`.
