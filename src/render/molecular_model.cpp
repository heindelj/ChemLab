#include "render/molecular_model.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

#include "raymath.h"
#include "rlgl.h"

#include "core/math_utils.h"

const char* RenderStyleName(RenderStyle style) {
    switch (style) {
        case RenderStyle::BallAndStick: return "ball-and-stick";
        case RenderStyle::Spheres: return "spheres";
        case RenderStyle::Sticks: return "sticks";
    }
    return "?";
}

bool ParseRenderStyle(const char* text, RenderStyle& out) {
    std::string s = text;
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    if (s == "ball-and-stick" || s == "ballstick" || s == "bs" || s == "ball_and_stick") { out = RenderStyle::BallAndStick; return true; }
    if (s == "spheres" || s == "spacefill" || s == "cpk" || s == "vdw") { out = RenderStyle::Spheres; return true; }
    if (s == "sticks" || s == "licorice" || s == "wire") { out = RenderStyle::Sticks; return true; }
    return false;
}

MolecularModel::~MolecularModel() { Unload(); }

void MolecularModel::Unload() {
    loaded = false;
    atomColors.clear();
    atomPositions.clear();
    atomRadii.clear();
    halfBonds.clear();
}

void MolecularModel::Build(const Atoms& atoms, const RenderSettings& settings) {
    Unload();
    loaded = true;
    atomColors.resize(atoms.natoms);
    for (uint32_t i = 0; i < atoms.natoms; ++i) atomColors[i] = atoms.renderData[i].color;
    UpdateGeometry(atoms, settings);
}

void MolecularModel::UpdateGeometry(const Atoms& atoms, const RenderSettings& settings) {
    atomPositions.resize(atoms.natoms);
    atomRadii.resize(atoms.natoms);
    if (atomColors.size() != atoms.natoms) {
        atomColors.resize(atoms.natoms);
        for (uint32_t i = 0; i < atoms.natoms; ++i) atomColors[i] = atoms.renderData[i].color;
    }

    const float sphereScale = settings.style == RenderStyle::Spheres ? settings.sphereScale
                            : settings.style == RenderStyle::Sticks ? 0.0f
                            : settings.ballScale;
    for (uint32_t i = 0; i < atoms.natoms; ++i) {
        float r = atoms.renderData[i].vdwRadius * sphereScale;
        if (settings.style == RenderStyle::Sticks) r = settings.stickRadius;  // round caps on the sticks
        atomRadii[i] = r;
        atomPositions[i] = atoms.xyz[i];
    }

    halfBonds.clear();
    halfBonds.reserve(atoms.covalentBondList.pairs.size() * 2);
    for (const auto& [a, b] : atoms.covalentBondList.pairs) {
        const Vector3 mid = midpoint(atoms.xyz[a], atoms.xyz[b]);
        if (norm(atoms.xyz[b] - atoms.xyz[a]) < 1e-6f) continue;
        halfBonds.push_back({a, b, atoms.xyz[a], mid});
        halfBonds.push_back({b, a, atoms.xyz[b], mid});
    }
}

// View-space depth of a world-space point (raylib matrices are column-major;
// row 3 of the view matrix is m2, m6, m10, m14). More negative = farther.
static inline float ViewDepth(const Matrix& view, const Vector3& p) {
    return view.m2 * p.x + view.m6 * p.y + view.m10 * p.z + view.m14;
}

static inline SphereInstanceGPU PackSphere(const Vector3& p, float r, Color c) {
    return SphereInstanceGPU{p.x, p.y, p.z, r, {c.r, c.g, c.b, c.a}};
}

void MolecularModel::Draw(const RenderSettings& settings) const {
    if (!loaded || atomPositions.empty()) return;

    sphOpaque.clear(); sphTransp.clear();
    cylOpaque.clear(); cylTransp.clear();

    // Spheres are drawn in every style: in "sticks" they are the round caps.
    for (size_t i = 0; i < atomPositions.size(); ++i) {
        const Color c = atomColors[i];
        if (c.a == 0 || atomRadii[i] <= 0.0f) continue;
        (c.a == 255 ? sphOpaque : sphTransp).push_back(PackSphere(atomPositions[i], atomRadii[i], c));
    }
    if (settings.style != RenderStyle::Spheres) {
        for (const HalfBond& hb : halfBonds) {
            Color c = settings.colorBonds ? atomColors[hb.atom] : settings.bondColor;
            c.a = atomColors[hb.atom].a;
            if (c.a == 0) continue;
            const CylinderInstanceGPU inst{hb.from.x, hb.from.y, hb.from.z, settings.stickRadius,
                                           hb.to.x, hb.to.y, hb.to.z,
                                           {c.r, c.g, c.b, c.a}};
            (c.a == 255 ? cylOpaque : cylTransp).push_back(inst);
        }
    }

    DrawSphereImpostors(sphOpaque.data(), (int)sphOpaque.size(), true);
    DrawCylinderImpostors(cylOpaque.data(), (int)cylOpaque.size(), true);

    if (!sphTransp.empty() || !cylTransp.empty()) {
        // Sort transparent impostors back-to-front (view-space z increases
        // toward the camera) and draw them without depth writes, so they
        // blend correctly against the opaque geometry and each other.
        const Matrix view = rlGetMatrixModelview();
        std::sort(sphTransp.begin(), sphTransp.end(), [&](const SphereInstanceGPU& a, const SphereInstanceGPU& b) {
            return ViewDepth(view, {a.x, a.y, a.z}) < ViewDepth(view, {b.x, b.y, b.z});
        });
        std::sort(cylTransp.begin(), cylTransp.end(), [&](const CylinderInstanceGPU& a, const CylinderInstanceGPU& b) {
            return ViewDepth(view, {0.5f * (a.ax + a.bx), 0.5f * (a.ay + a.by), 0.5f * (a.az + a.bz)})
                 < ViewDepth(view, {0.5f * (b.ax + b.bx), 0.5f * (b.ay + b.by), 0.5f * (b.az + b.bz)});
        });
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        DrawCylinderImpostors(cylTransp.data(), (int)cylTransp.size(), true);
        DrawSphereImpostors(sphTransp.data(), (int)sphTransp.size(), true);
        rlEnableDepthMask();
    }
}

void MolecularModel::DrawHighlighted(const std::set<int>& atomIndices, const RenderSettings& settings) const {
    if (!loaded || atomIndices.empty()) return;
    highlightScratch.clear();
    for (int i : atomIndices) {
        if (i < 0 || i >= (int)atomPositions.size()) continue;
        const float r = atomRadii[i] * 1.12f + 0.03f;
        highlightScratch.push_back(PackSphere(atomPositions[i], r, settings.highlightColor));
    }
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    DrawSphereImpostors(highlightScratch.data(), (int)highlightScratch.size(), false);
    rlEnableDepthMask();
}

int MolecularModel::PickAtom(Ray ray) const {
    int best = -1;
    float bestDist = FLT_MAX;
    for (size_t i = 0; i < atomPositions.size(); ++i) {
        if (atomColors[i].a == 0 || atomRadii[i] <= 0.0f) continue;
        const RayCollision hit = GetRayCollisionSphere(ray, atomPositions[i], atomRadii[i]);
        if (hit.hit && hit.distance < bestDist) {
            bestDist = hit.distance;
            best = (int)i;
        }
    }
    return best;
}
