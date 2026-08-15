# Simple Molecular Visulization

Currently, chemlab is a simple, efficient software for visualizing molecular structures.
Here are some of the available features:

- Visualize molecules instantly from the command-line: `chemlab input.xyz`
- Easily measure distances, angles, and dihedrals by double-clicking on atoms
- Customize color of atoms with color-picker
- Export high-quality PNGs

## Building on macOS

Requirements:

- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.16 or newer

The first configure downloads pinned versions of raylib and fmt:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If `cmake` is installed by Homebrew but your shell cannot find it, either add Homebrew to your PATH:

```sh
export PATH="/opt/homebrew/bin:$PATH"
```

or call it directly:

```sh
/opt/homebrew/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
/opt/homebrew/bin/cmake --build build -j
```

Run ChemLab with an xyz file:

```sh
./build/ChemLab assets/caffeine.xyz
```
