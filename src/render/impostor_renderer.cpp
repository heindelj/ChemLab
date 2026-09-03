#include "render/impostor_renderer.h"

#include <cstddef>
#include <string>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
// All impostor math happens in view space with the eye at the origin, which
// keeps the ray equations trivial (ray = t * normalize(fragViewPos)) and makes
// the camera-attached headlight a constant. The existing mesh renderer lit the
// scene with one directional light placed at the camera and aimed at the
// target (see Viewport::UpdateLighting), i.e. a headlight: in view space that
// light vector is exactly (0, 0, 1), so the shading below reproduces the old
// look -- same NdotL term, same pow-16 specular divided by 10, same 0.2
// ambient and 1/2.2 gamma -- without needing any light uniforms.

// GL 3.3 core on desktop, GLSL ES 3.00 (WebGL2) on the web. Everything these
// shaders rely on -- gl_FragDepth, `flat` varyings, instanced arrays -- is
// core in ES 3.00, so the bodies below are shared verbatim; ES only adds the
// requirement that precision be declared.
#if defined(__EMSCRIPTEN__)
static const char* kGlslVersion = "#version 300 es\nprecision highp float;\nprecision highp int;\n";
#else
static const char* kGlslVersion = "#version 330\n";
#endif

// Shared fragment-shader helper: shade a view-space hit point.
static const char* kShadeCommon = R"(
uniform int lit;
vec4 ShadeImpostor(vec3 hitView, vec3 n, vec4 base)
{
    if (lit == 0) return base;
    vec3 viewD = -normalize(hitView);
    vec3 lightDir = vec3(0.0, 0.0, 1.0);   // camera headlight (view space)
    float NdotL = max(dot(n, lightDir), 0.0);
    vec3 lightDot = vec3(NdotL);           // white light
    float specCo = 0.0;
    if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-lightDir, n))), 16.0);
    vec3 specular = vec3(specCo / 10.0);
    vec4 outColor = (base + vec4(specular, 1.0)) * vec4(lightDot, 1.0);
    outColor += base * vec4(0.2, 0.2, 0.2, 1.0);      // ambient
    outColor = pow(max(outColor, vec4(0.0)), vec4(1.0 / 2.2));
    outColor.a = base.a;
    return outColor;
}
)";

// --- Sphere impostor ---
// The billboard is placed in the plane of the silhouette (the circle where
// the tangent cone from the eye touches the sphere): center dir*t with
// t = (d^2 - r^2)/d and radius rc = r*sqrt(d^2 - r^2)/d. A quad of half-size
// rc in that plane bounds the silhouette exactly, so there is no overdraw
// beyond the projected circle's bounding square and no clipping at any FOV.
static const char* kSphereVS = R"(
in vec2 quadPos;          // [-1,1] billboard corner
in vec4 instPosRadius;    // world center, radius
in vec4 instColor;
uniform mat4 matView;
uniform mat4 matProj;
out vec3 fragViewPos;
flat out vec3 impCenter;
flat out float impRadius;
flat out vec4 impColor;
void main()
{
    vec3 c = (matView * vec4(instPosRadius.xyz, 1.0)).xyz;
    float r = instPosRadius.w;
    impCenter = c;
    impRadius = r;
    impColor = instColor;
    float d2 = dot(c, c);
    float r2 = r * r;
    if (r <= 0.0 || d2 <= r2 * 1.02) {
        // Degenerate or camera inside the sphere: emit a clipped vertex.
        // (The old mesh renderer also showed nothing from inside an atom.)
        fragViewPos = vec3(0.0);
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    float d = sqrt(d2);
    vec3 dir = c / d;
    float t = (d2 - r2) / d;                 // silhouette plane distance
    float rc = r * sqrt(d2 - r2) / d;        // silhouette radius
    vec3 refUp = (abs(dir.y) < 0.95) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(dir, refUp));
    vec3 up = cross(right, dir);
    vec3 p = dir * t + (right * quadPos.x + up * quadPos.y) * rc;
    fragViewPos = p;
    gl_Position = matProj * vec4(p, 1.0);
}
)";

static const char* kSphereFS = R"(
in vec3 fragViewPos;
flat in vec3 impCenter;
flat in float impRadius;
flat in vec4 impColor;
uniform mat4 matProj;
out vec4 finalColor;
void main()
{
    vec3 rd = normalize(fragViewPos);
    float b = dot(rd, impCenter);
    float disc = b * b - dot(impCenter, impCenter) + impRadius * impRadius;
    if (disc < 0.0) discard;
    float thit = b - sqrt(disc);
    if (thit < 0.0) discard;
    vec3 hit = rd * thit;
    vec3 n = (hit - impCenter) / impRadius;
    vec4 clip = matProj * vec4(hit, 1.0);
    gl_FragDepth = clamp((clip.z / clip.w) * 0.5 + 0.5, 0.0, 1.0);
    finalColor = ShadeImpostor(hit, n, impColor);
}
)";

// --- Cylinder impostor ---
// The instanced geometry is the cylinder's oriented bounding box (36 vertices,
// wound CCW so default backface culling works); the fragment shader ray-casts
// a finite cylinder with flat end caps. Using a box rather than a
// view-aligned quad avoids every degenerate viewing angle (end-on bonds
// become the box's front face).
static const char* kCylinderVS = R"(
in vec3 boxPos;           // [-1,1]^3 box corner
in vec4 instA;            // world endpoint A, radius
in vec3 instB;            // world endpoint B
in vec4 instColor;
uniform mat4 matView;
uniform mat4 matProj;
out vec3 fragViewPos;
flat out vec3 cylA;
flat out vec3 cylB;
flat out float cylR;
flat out vec4 impColor;
void main()
{
    vec3 a = (matView * vec4(instA.xyz, 1.0)).xyz;
    vec3 b = (matView * vec4(instB, 1.0)).xyz;
    float r = instA.w;
    cylA = a;
    cylB = b;
    cylR = r;
    impColor = instColor;
    vec3 axis = b - a;
    float len = length(axis);
    if (r <= 0.0 || len <= 0.0) {
        fragViewPos = vec3(0.0);
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    vec3 u = axis / len;
    vec3 refUp = (abs(u.y) < 0.95) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 n1 = normalize(cross(u, refUp));
    vec3 n2 = cross(u, n1);              // (u, n1, n2) right-handed
    vec3 mid = 0.5 * (a + b);
    vec3 p = mid + u * (boxPos.x * 0.5 * len) + n1 * (boxPos.y * r) + n2 * (boxPos.z * r);
    fragViewPos = p;
    gl_Position = matProj * vec4(p, 1.0);
}
)";

static const char* kCylinderFS = R"(
in vec3 fragViewPos;
flat in vec3 cylA;
flat in vec3 cylB;
flat in float cylR;
flat in vec4 impColor;
uniform mat4 matProj;
out vec4 finalColor;
void main()
{
    vec3 rd = normalize(fragViewPos);
    vec3 ba = cylB - cylA;
    vec3 oc = -cylA;                     // eye (origin) relative to A
    float baba = dot(ba, ba);
    float bard = dot(ba, rd);
    float baoc = dot(ba, oc);
    float k2 = baba - bard * bard;
    float k1 = baba * dot(oc, rd) - baoc * bard;
    float k0 = baba * dot(oc, oc) - baoc * baoc - cylR * cylR * baba;
    float h = k1 * k1 - k2 * k0;
    if (h < 0.0) discard;
    h = sqrt(h);
    float t = 0.0;
    vec3 n = vec3(0.0);
    bool ok = false;
    if (abs(k2) > 1e-7) {                // body of the cylinder
        float tb = (-k1 - h) / k2;
        float y = baoc + tb * bard;
        if (tb > 0.0 && y > 0.0 && y < baba) {
            t = tb;
            n = (oc + tb * rd - ba * (y / baba)) / cylR;
            ok = true;
        }
    }
    if (!ok && abs(bard) > 1e-7) {       // flat end caps
        float t1 = (0.0 - baoc) / bard;
        float t2 = (baba - baoc) / bard;
        float tn = min(t1, t2);
        float tf = max(t1, t2);
        for (int i = 0; i < 2; i++) {
            float tc = (i == 0) ? tn : tf;
            if (ok || tc <= 0.0) continue;
            vec3 pa = tc * rd + oc;      // hit relative to A
            float y = dot(pa, ba);
            vec3 q = pa - ((y > 0.5 * baba) ? ba : vec3(0.0));
            if (dot(q, q) <= cylR * cylR) {
                t = tc;
                n = normalize(ba) * ((y > 0.5 * baba) ? 1.0 : -1.0);
                ok = true;
            }
        }
    }
    if (!ok) discard;
    vec3 hit = rd * t;
    vec4 clip = matProj * vec4(hit, 1.0);
    gl_FragDepth = clamp((clip.z / clip.w) * 0.5 + 0.5, 0.0, 1.0);
    finalColor = ShadeImpostor(hit, n, impColor);
}
)";

// ---------------------------------------------------------------------------
// GL plumbing
// ---------------------------------------------------------------------------

namespace {

struct InstanceAttrib {
    int location;      // shader attribute location (-1 = unused)
    int components;    // 1..4
    int glType;        // RL_FLOAT / RL_UNSIGNED_BYTE
    bool normalized;
    int offset;        // byte offset within the instance struct
};

struct ImpostorBatch {
    Shader shader{};
    unsigned int vao = 0;
    unsigned int baseVbo = 0;       // per-vertex billboard geometry
    unsigned int instVbo = 0;       // streamed per-instance data
    int instCapacity = 0;           // bytes
    int instStride = 0;
    int vertsPerInstance = 0;
    int locMatView = -1, locMatProj = -1, locLit = -1;
    InstanceAttrib attribs[3] = {};
    int numAttribs = 0;
};

ImpostorBatch gSpheres;
ImpostorBatch gCylinders;
bool gReady = false;

void ConfigureInstanceAttribs(ImpostorBatch& b) {
    // VAO and instance VBO must be bound.
    for (int i = 0; i < b.numAttribs; ++i) {
        const InstanceAttrib& a = b.attribs[i];
        if (a.location < 0) continue;
        rlSetVertexAttribute((unsigned int)a.location, a.components, a.glType,
                             a.normalized, b.instStride, a.offset);
        rlEnableVertexAttribute((unsigned int)a.location);
        rlSetVertexAttributeDivisor((unsigned int)a.location, 1);
    }
}

void EnsureInstanceCapacity(ImpostorBatch& b, int bytes) {
    if (b.instCapacity >= bytes) return;
    int newCap = b.instCapacity > 0 ? b.instCapacity : 16 * 1024;
    while (newCap < bytes) newCap *= 2;
    rlEnableVertexArray(b.vao);
    if (b.instVbo != 0) rlUnloadVertexBuffer(b.instVbo);
    b.instVbo = rlLoadVertexBuffer(nullptr, newCap, true);   // dynamic
    ConfigureInstanceAttribs(b);
    rlDisableVertexArray();
    b.instCapacity = newCap;
}

void BuildBatch(ImpostorBatch& b, const char* vsBody, const char* fsBody,
                const float* baseVerts, int baseFloatsPerVert, int vertCount,
                const char* baseAttribName) {
    std::string vs = std::string(kGlslVersion) + vsBody;
    std::string fs = std::string(kGlslVersion) + kShadeCommon + fsBody;
    b.shader = LoadShaderFromMemory(vs.c_str(), fs.c_str());
    b.locMatView = GetShaderLocation(b.shader, "matView");
    b.locMatProj = GetShaderLocation(b.shader, "matProj");
    b.locLit = GetShaderLocation(b.shader, "lit");
    b.vertsPerInstance = vertCount;

    b.vao = rlLoadVertexArray();
    rlEnableVertexArray(b.vao);
    b.baseVbo = rlLoadVertexBuffer(baseVerts, vertCount * baseFloatsPerVert * (int)sizeof(float), false);
    const int baseLoc = rlGetLocationAttrib(b.shader.id, baseAttribName);
    if (baseLoc >= 0) {
        rlSetVertexAttribute((unsigned int)baseLoc, baseFloatsPerVert, RL_FLOAT, false,
                             baseFloatsPerVert * (int)sizeof(float), 0);
        rlEnableVertexAttribute((unsigned int)baseLoc);
    }
    rlDisableVertexArray();
}

void UnloadBatch(ImpostorBatch& b) {
    if (b.vao != 0) rlUnloadVertexArray(b.vao);
    if (b.baseVbo != 0) rlUnloadVertexBuffer(b.baseVbo);
    if (b.instVbo != 0) rlUnloadVertexBuffer(b.instVbo);
    if (b.shader.id != 0) UnloadShader(b.shader);
    b = ImpostorBatch{};
}

void DrawBatch(ImpostorBatch& b, const void* data, int count, bool lit) {
    if (count <= 0 || !gReady) return;
    // Flush raylib's pending batched geometry so draw order stays sane.
    rlDrawRenderBatchActive();

    EnsureInstanceCapacity(b, count * b.instStride);
    rlUpdateVertexBuffer(b.instVbo, data, count * b.instStride, 0);

    const Matrix view = rlGetMatrixModelview();
    const Matrix proj = rlGetMatrixProjection();

    rlEnableShader(b.shader.id);
    rlSetUniformMatrix(b.locMatView, view);
    rlSetUniformMatrix(b.locMatProj, proj);
    const int litInt = lit ? 1 : 0;
    rlSetUniform(b.locLit, &litInt, RL_SHADER_UNIFORM_INT, 1);

    rlEnableVertexArray(b.vao);
    rlDrawVertexArrayInstanced(0, b.vertsPerInstance, count);
    rlDisableVertexArray();
    rlDisableShader();
}

}  // namespace

// Reference-counted: every Viewport calls Init/Shutdown, and there is now one
// Viewport per 3D window (the Structure View plus each Render 3D node window),
// all sharing these batches.
static int gRefs = 0;

void InitImpostorRenderer() {
    ++gRefs;
    if (gReady) return;

    // Sphere billboard: two CCW triangles facing the camera.
    static const float quad[] = {
        -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
        -1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f,
    };
    BuildBatch(gSpheres, kSphereVS, kSphereFS, quad, 2, 6, "quadPos");
    gSpheres.instStride = (int)sizeof(SphereInstanceGPU);
    gSpheres.attribs[0] = {rlGetLocationAttrib(gSpheres.shader.id, "instPosRadius"), 4, RL_FLOAT, false, 0};
    gSpheres.attribs[1] = {rlGetLocationAttrib(gSpheres.shader.id, "instColor"), 4, RL_UNSIGNED_BYTE, true,
                           (int)offsetof(SphereInstanceGPU, rgba)};
    gSpheres.numAttribs = 2;

    // Cylinder bounding box: 12 CCW triangles (outward-facing, so default
    // backface culling draws each covered pixel exactly once -- required for
    // correct transparency).
    static const float box[] = {
        // +X
         1,-1,-1,  1, 1,-1,  1, 1, 1,   1,-1,-1,  1, 1, 1,  1,-1, 1,
        // -X
        -1,-1,-1, -1, 1, 1, -1, 1,-1,  -1,-1,-1, -1,-1, 1, -1, 1, 1,
        // +Y
        -1, 1,-1, -1, 1, 1,  1, 1, 1,  -1, 1,-1,  1, 1, 1,  1, 1,-1,
        // -Y
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  -1,-1,-1,  1,-1, 1, -1,-1, 1,
        // +Z
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  -1,-1, 1,  1, 1, 1, -1, 1, 1,
        // -Z
        -1,-1,-1, -1, 1,-1,  1, 1,-1,  -1,-1,-1,  1, 1,-1,  1,-1,-1,
    };
    BuildBatch(gCylinders, kCylinderVS, kCylinderFS, box, 3, 36, "boxPos");
    gCylinders.instStride = (int)sizeof(CylinderInstanceGPU);
    gCylinders.attribs[0] = {rlGetLocationAttrib(gCylinders.shader.id, "instA"), 4, RL_FLOAT, false, 0};
    gCylinders.attribs[1] = {rlGetLocationAttrib(gCylinders.shader.id, "instB"), 3, RL_FLOAT, false,
                             (int)offsetof(CylinderInstanceGPU, bx)};
    gCylinders.attribs[2] = {rlGetLocationAttrib(gCylinders.shader.id, "instColor"), 4, RL_UNSIGNED_BYTE, true,
                             (int)offsetof(CylinderInstanceGPU, rgba)};
    gCylinders.numAttribs = 3;

    gReady = true;
}

void ShutdownImpostorRenderer() {
    if (gRefs > 0 && --gRefs > 0) return;
    if (!gReady) return;
    UnloadBatch(gSpheres);
    UnloadBatch(gCylinders);
    gReady = false;
}

void DrawSphereImpostors(const SphereInstanceGPU* instances, int count, bool lit) {
    DrawBatch(gSpheres, instances, count, lit);
}

void DrawCylinderImpostors(const CylinderInstanceGPU* instances, int count, bool lit) {
    DrawBatch(gCylinders, instances, count, lit);
}
