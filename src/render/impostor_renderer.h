#pragma once
// GPU impostor rendering for spheres and cylinders: instead of tessellated
// meshes, each primitive is one instanced billboard (a quad for spheres, a
// bounding box for cylinders) whose exact surface is ray-cast analytically in
// the fragment shader. gl_FragDepth is written per fragment, so impostors
// depth-intersect each other and ordinary raylib geometry (e.g. the grid)
// correctly. Vertex cost is 6 vertices per atom and 36 per half-bond
// regardless of screen size, and every batch is a single draw call, so scenes
// with hundreds of thousands of atoms render at interactive rates.
//
// Perspective cameras only (matches OrbitCamera). Requires GLSL 330 (desktop).

#include <cstdint>

// Per-instance data, tightly packed to match the GL attribute layout.
struct SphereInstanceGPU {
    float x, y, z, r;         // world-space center + radius
    unsigned char rgba[4];
};
static_assert(sizeof(SphereInstanceGPU) == 20, "packed layout expected");

struct CylinderInstanceGPU {
    float ax, ay, az, r;      // world-space endpoint A + radius (one vec4 attribute)
    float bx, by, bz;         // world-space endpoint B
    unsigned char rgba[4];
};
static_assert(sizeof(CylinderInstanceGPU) == 32, "packed layout expected");

// Lifetime is tied to the GL context: init after InitWindow, shutdown before
// CloseWindow. Both are idempotent.
void InitImpostorRenderer();
void ShutdownImpostorRenderer();

// Draw a batch of impostors in one instanced call. Must be called between
// BeginMode3D/EndMode3D (the current rlgl view/projection matrices are used).
// `lit` = false draws flat unlit color (used for selection highlights).
// Instances are drawn in array order, which is what makes sorted transparency
// possible: pass a back-to-front sorted array with depth writes disabled.
void DrawSphereImpostors(const SphereInstanceGPU* instances, int count, bool lit);
void DrawCylinderImpostors(const CylinderInstanceGPU* instances, int count, bool lit);
