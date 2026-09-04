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
`View > Reset layout` restores the scene's arrangement (see *Scenes* below). A layout saved by an older
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

### Node graph and plots

The Node Graph panel wires data sources, scripts and analyses together
(`graph demo distance|highlight|plots` builds an example, right-click the
canvas to add nodes). The *Data* and *Plot* nodes turn tabular data into
plots without leaving the app:

```
Load Table (csv/tsv/whitespace) -> Column -> Series (line, scatter, bars,
    stairs, stems, histogram) -> Plot 2D (a *named* plot)
```

Every `Plot 2D` node publishes its plot under a name; the dropdown floating
in the 2D Plot panel (or `plot <name>`, `plot list`) switches between the
built-in per-frame plots and the published ones. The built-in **Plot Lab** UI
(`scene plot-lab`, or `graph demo plots`, which also loads `assets/data/md_demo.csv`)
shows the live plot on top and the graph that feeds it below.

### Scenes

The arrangement on screen is a *scene*: a graph containing a **Layout** node.
A Layout node picks a layout (how the dockspace is carved into slots) and has
one input pin per slot; **Panel** nodes (one per panel type) plug into those
pins, directly or through a **Tabs** node (several panels in one slot become
tabs, top input first). What is wired into the Layout node is what the screen
shows, and it follows every edit. The UI Builder (`scene builder`,
`View > Scene > UI Builder...`) edits exactly the same wiring by drag and
drop, so building a scene visually and wiring nodes in the graph editor are
one thing done two ways.

Every scene is a graph, but a graph is only a scene while it has a Layout
node; the scene is named after its first Layout node, and the graph itself
can be called something else (the file name). A scene may hold several Layout
nodes -- its *layouts* -- and switch between them; they can share Panel
nodes, so a panel set up once appears in several arrangements. A Layout
node's layout is locked while anything is wired into its slots (unplug the
slots to change it). Two scenes are built in: `classic` (drawn above) and
`plot-lab` (2D plot over the node graph), one layout each; a scene with
several layouts gets a layout picker next to the Graph button and in its
scene graph window.

```
scene                         list scenes and their layouts (* = on screen)
scene classic                 show the classic scene (a layout name of any scene works too)
scene layout wide             switch layouts within the current scene
scene layout add rdf quad     add an empty "rdf" layout on the "quad" layout to the current scene
scene classic graph           open its scene graph (the Graph button top right does the same for the current scene)
scene new bench two-column    a new scene with one empty layout
scene save                    write the current scene to scenes/<graph-name>.json
```

`View > Scene` lists the same things (scenes with several layouts get a
submenu), and each Layout node has *Show* and *UI Builder* buttons. Any other
node can be added to a scene graph and run from its window; a Render 3D or
Plot 2D node there opens a window of its own. User scenes in `scenes/` are
loaded at startup and the scene/layout shown last is remembered in
`chemlab_scene.toml`. `ui` is an alias of `scene`.

## Projects

A project is a folder with a `chemlab.toml` at its root. While it is open the
folder is the working directory, so bare file names everywhere (`load`,
`screenshot`, `export`, script nodes, file dialogs) resolve inside it, and
everything the project references is stored relative to it, so it can be
moved, zipped or committed as a unit:

```
my_project/
  chemlab.toml     # name, folders, scene, view settings, python, structures, startup commands
  layout.ini       # the dock layout, written by ChemLab for this project
  data/            # structures; bare names in [[structures]] and `load` are searched here
  scenes/          # scene graphs (`scene save`), replaces the global scenes/ while open
  graphs/          # named node graphs (`graph save`)
  scripts/         # python node scripts; a bare script name in a graph is looked up here
  output/          # where `screenshot` / `export` land when given a bare name
```

`File > New project...` adopts whatever is loaded (and the scene on screen)
into a new folder, `File > Open project...` (or `./build/ChemLab my_project`)
restores it, and `Ctrl+Shift+S` saves. The same is available as
`project new|open|save|close|info` on the command bar. Switching scenes or
changing view settings marks the project dirty (`*` in the title) until saved;
`project close` restores the previous working directory. The file is written
with comments and meant to be hand-edited:

```toml
[project]
name = "Water cages"

[paths]                       # all optional; these are the defaults
data = ["data"]               # a list: add a shared trajectory store outside the project
scenes = "scenes"
graphs = "graphs"
scripts = "scripts"
output = "output"
layout = "layout.ini"

[scene]
active = "plot-lab"           # built-in or scenes/<name>.json
layout = ""                   # a layout within the scene ("" = its first)

[view]
style = "sticks"
background = "#202030"

[python]
interpreter = ".venv/bin/python"   # relative to the project, or a command on PATH
env = { OMP_NUM_THREADS = "4" }

[[structures]]
path = "w20.xyz"              # found in data/
frame = 42
measurements = [[1, 4], [1, 4, 7]]

[startup]
commands = ["plot measurements", "rotate on 15"]
```

`[scripts] files = [...]` lists the project's scripts (relative to
`paths.scripts`); `project info` shows them along with every resolved folder.

## Source layout

```
src/core     molecule data, xyz IO, geometry, element tables (no rendering)
src/render   lighting shader, GPU molecular model, orbit camera, off-screen viewport
src/app      AppState, actions (every state change), the command registry, projects (chemlab.toml)
src/graph    node graph: values, node types (built-in, data/plot, python), evaluation, script protocol
src/plot     UI-free description of a 2D plot (series, axes); named plots live in AppState::plots
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

### Web build (WebAssembly)

ChemLab also builds for the browser with Emscripten. The impostor renderer
needs `gl_FragDepth`, `flat` varyings and instanced arrays, so the web build
targets **WebGL2 / GLSL ES 3.00 only** (raylib is configured with
`OPENGL_VERSION="ES 3.0"`, which makes it pass `-sMIN/MAX_WEBGL_VERSION=2`).
WebGL1 is not supported and there is no fallback.

```sh
# once: https://emscripten.org/docs/getting_started/downloads.html
source /path/to/emsdk/emsdk_env.sh

emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
python3 -m http.server --directory build-web 8000   # then open localhost:8000/ChemLab.html
```

The output is `build-web/ChemLab.{html,js,wasm,data}`; it must be served over
HTTP, not opened as a `file://` URL. `web/shell.html` is the page template.

Differences from the desktop build:

- `assets/` is baked into `ChemLab.data` and mounted at `/assets`, so the
  bundled samples load as usual and `caffeine.xyz` still opens on startup.
- The native file dialogs are compiled out (`portable-file-dialogs` spawns a
  helper process). **Drag an `.xyz` file onto the canvas** to load it; the
  dialog buttons are inert. Screenshots and exports write into the in-memory
  filesystem, i.e. they are effectively unavailable.
- ImGui's `.ini` layout lives in the in-memory filesystem and is lost on
  reload. Wiring it to IDBFS would make layouts persist.
- The frame body is `RunFrame()` in `src/main.cpp`, driven by
  `emscripten_set_main_loop_arg()`; raylib's `WindowShouldClose()` calls
  `emscripten_sleep()` and would need ASYNCIFY, so the web build never calls it.
- Use a current emsdk. The code uses parenthesized aggregate initialisation
  (`Color(r,g,b,a)` in `src/core/atomic_data.h`), which needs Clang 16+; older
  toolchains want brace initialisation instead.
- fmt is pinned at 12.2.0 rather than 11.2.0: 11.2.0's `format.h` calls
  `malloc`/`free` without including `<cstdlib>` and only compiles where some
  other header drags it in, which recent libc++ (emsdk's clang) no longer does.
