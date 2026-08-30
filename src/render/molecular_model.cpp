#include "render/molecular_model.h"

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
    if (!loaded) return;
    UnloadMesh(sphereMesh);
    UnloadMesh(stickMesh);
    // Not UnloadMaterial(): that would also unload the shared lighting shader.
    // Only the map arrays belong to us.
    MemFree(material.maps);
    MemFree(outlineMaterial.maps);
    material = Material{};
    outlineMaterial = Material{};
    loaded = false;
    atomColors.clear();
    atomTransforms.clear();
    atomRadii.clear();
    halfBonds.clear();
}

void MolecularModel::Build(const Atoms& atoms, Shader shader, const RenderSettings& settings) {
    Unload();
    sphereMesh = GenMeshSphere(1.0f, 20, 20);
    stickMesh = GenMeshCylinder(1.0f, 1.0f, 16);  // unit radius; scaled per draw
    material = LoadMaterialDefault();
    material.shader = shader;
    outlineMaterial = LoadMaterialDefault();
    loaded = true;

    atomColors.resize(atoms.natoms);
    for (uint32_t i = 0; i < atoms.natoms; ++i) atomColors[i] = atoms.renderData[i].color;
    UpdateGeometry(atoms, settings);
}

void MolecularModel::UpdateGeometry(const Atoms& atoms, const RenderSettings& settings) {
    atomTransforms.resize(atoms.natoms);
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
        atomTransforms[i] = MatrixScale(r) * MatrixTranslate(atoms.xyz[i]);
    }

    halfBonds.clear();
    halfBonds.reserve(atoms.covalentBondList.pairs.size() * 2);
    for (const auto& [a, b] : atoms.covalentBondList.pairs) {
        const Vector3 bondVector = atoms.xyz[b] - atoms.xyz[a];
        const float len = norm(bondVector);
        if (len < 1e-6f) continue;
        const Matrix align = MatrixAlignToAxis(Vector3{0, 1, 0}, bondVector);
        const Matrix scale = MatrixScale(Vector3{settings.stickRadius, 0.5f * len, settings.stickRadius});
        halfBonds.push_back({a, b, scale * align * MatrixTranslate(atoms.xyz[a])});
        halfBonds.push_back({b, a, scale * align * MatrixTranslate(atoms.xyz[a] + 0.5f * bondVector)});
    }
}

void MolecularModel::Draw(const RenderSettings& settings) const {
    if (!loaded) return;
    Material mat = material;  // copy so we can poke the diffuse colour per draw
    // Spheres are drawn in every style: in "sticks" they are the round caps.
    for (size_t i = 0; i < atomTransforms.size(); ++i) {
        mat.maps[MATERIAL_MAP_DIFFUSE].color = atomColors[i];
        DrawMesh(sphereMesh, mat, atomTransforms[i]);
    }
    if (settings.style != RenderStyle::Spheres) {
        for (const HalfBond& hb : halfBonds) {
            Color c = settings.colorBonds ? atomColors[hb.atom] : settings.bondColor;
            c.a = atomColors[hb.atom].a;
            mat.maps[MATERIAL_MAP_DIFFUSE].color = c;
            DrawMesh(stickMesh, mat, hb.transform);
        }
    }
}

void MolecularModel::DrawHighlighted(const std::set<int>& atomIndices, const RenderSettings& settings) const {
    if (!loaded || atomIndices.empty()) return;
    rlDisableDepthMask();
    outlineMaterial.maps[MATERIAL_MAP_DIFFUSE].color = settings.highlightColor;
    for (int i : atomIndices) {
        if (i < 0 || i >= (int)atomTransforms.size()) continue;
        const Vector3 pos = PositionVectorFromTransform(atomTransforms[i]);
        const float r = atomRadii[i] * 1.12f + 0.03f;
        DrawMesh(sphereMesh, outlineMaterial, MatrixScale(r) * MatrixTranslate(pos));
    }
    rlEnableDepthMask();
}

int MolecularModel::PickAtom(Ray ray) const {
    int best = -1;
    float bestDist = FLT_MAX;
    for (size_t i = 0; i < atomTransforms.size(); ++i) {
        const Vector3 pos = PositionVectorFromTransform(atomTransforms[i]);
        const RayCollision hit = GetRayCollisionSphere(ray, pos, atomRadii[i]);
        if (hit.hit && hit.distance < bestDist) {
            bestDist = hit.distance;
            best = (int)i;
        }
    }
    return best;
}
