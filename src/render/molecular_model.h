#pragma once
// GPU-side representation of one frame, drawn with ray-cast impostors (see
// impostor_renderer.h): per-atom center/radius/colour and per-half-bond
// endpoints. Colours live per atom so the UI can recolour a selection without
// touching the geometry. Everything is batched into a handful of instanced
// draw calls, so the cost per frame is O(atoms) CPU-side packing plus a
// constant number of GPU submissions.

#include <set>
#include <vector>

#include "raylib.h"

#include "core/molecule.h"
#include "render/impostor_renderer.h"

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
    Vector3 from;       // atom position
    Vector3 to;         // bond midpoint
};

class MolecularModel {
public:
    MolecularModel() = default;
    ~MolecularModel();
    MolecularModel(const MolecularModel&) = delete;
    MolecularModel& operator=(const MolecularModel&) = delete;

    void Build(const Atoms& atoms, const RenderSettings& settings);
    // Update geometry after positions/bonds changed, keeping colours.
    void UpdateGeometry(const Atoms& atoms, const RenderSettings& settings);
    void Unload();

    // Must be called inside BeginMode3D. Opaque impostors draw first; any
    // atoms with alpha < 255 are depth-sorted back-to-front and blended with
    // depth writes off.
    void Draw(const RenderSettings& settings) const;
    void DrawHighlighted(const std::set<int>& atomIndices, const RenderSettings& settings) const;

    // Returns the index of the closest atom hit by the ray or -1.
    int PickAtom(Ray ray) const;

    uint32_t AtomCount() const { return static_cast<uint32_t>(atomPositions.size()); }
    bool IsLoaded() const { return loaded; }

    std::vector<Color> atomColors;      // editable by the UI
    std::vector<Vector3> atomPositions;
    std::vector<float> atomRadii;       // world-space radius of each drawn sphere
    std::vector<HalfBond> halfBonds;

private:
    bool loaded = false;

    // Per-frame packing scratch, kept to avoid reallocating every draw.
    mutable std::vector<SphereInstanceGPU> sphOpaque, sphTransp;
    mutable std::vector<CylinderInstanceGPU> cylOpaque, cylTransp;
    mutable std::vector<SphereInstanceGPU> highlightScratch;
    mutable std::vector<float> sphDepthScratch, cylDepthScratch;   // transparent-pass sort keys
};
