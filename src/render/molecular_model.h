#pragma once
// GPU-side representation of one frame: shared sphere/cylinder meshes plus a
// per-atom / per-half-bond transform and colour. Colours live per atom so the
// UI can recolour a selection without touching the mesh data.

#include <set>
#include <vector>

#include "raylib.h"

#include "core/molecule.h"

enum class RenderStyle { BallAndStick = 0, Spheres = 1, Sticks = 2 };

const char* RenderStyleName(RenderStyle style);
bool ParseRenderStyle(const char* text, RenderStyle& out);

struct RenderSettings {
    RenderStyle style = RenderStyle::BallAndStick;
    float ballScale = 0.25f;     // multiplied by the vdW radius in ball-and-stick
    float stickRadius = 0.2f;    // cylinder radius (angstrom)
    float sphereScale = 1.0f;    // multiplied by the vdW radius in space-filling
    bool colorBonds = true;      // half-bonds take the colour of their atom
    Color bondColor = Color{160, 160, 160, 255};
    Color highlightColor = YELLOW;
};

struct HalfBond {
    uint32_t atom;      // atom this half belongs to
    uint32_t partner;
    Matrix transform;
};

class MolecularModel {
public:
    MolecularModel() = default;
    ~MolecularModel();
    MolecularModel(const MolecularModel&) = delete;
    MolecularModel& operator=(const MolecularModel&) = delete;

    void Build(const Atoms& atoms, Shader shader, const RenderSettings& settings);
    // Update transforms after positions/bonds changed, keeping colours.
    void UpdateGeometry(const Atoms& atoms, const RenderSettings& settings);
    void Unload();

    void Draw(const RenderSettings& settings) const;
    void DrawHighlighted(const std::set<int>& atomIndices, const RenderSettings& settings) const;

    // Returns the index of the closest atom hit by the ray or -1.
    int PickAtom(Ray ray) const;

    uint32_t AtomCount() const { return static_cast<uint32_t>(atomTransforms.size()); }
    bool IsLoaded() const { return loaded; }

    std::vector<Color> atomColors;   // editable by the UI
    std::vector<Matrix> atomTransforms;
    std::vector<float> atomRadii;    // world-space radius of each drawn sphere
    std::vector<HalfBond> halfBonds;

private:
    bool loaded = false;
    Mesh sphereMesh{};
    Mesh stickMesh{};
    Material material{};          // lit, colour poked per draw call
    Material outlineMaterial{};   // unlit, for selection rings
};
