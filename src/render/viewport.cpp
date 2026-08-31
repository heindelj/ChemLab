#define RLIGHTS_IMPLEMENTATION
#include "render/viewport.h"

#include "raymath.h"
#include "rlgl.h"

#include "core/math_utils.h"
#include "render/impostor_renderer.h"
#include "render/shaders.h"

void FixAppleScreenScale() {
#if defined(__APPLE__)
    // Re-run what raylib's SetupViewport() does on Apple, from the *current*
    // content scale: the GL viewport must cover the whole framebuffer
    // (logical size x backing scale) while the projection stays in logical
    // points. raylib only recomputes this on a window-size event, so a window
    // dragged to a monitor with a different scale keeps a stale viewport.
    const Vector2 scale = GetWindowScaleDPI();
    const int w = GetScreenWidth(), h = GetScreenHeight();
    rlDrawRenderBatchActive();
    rlViewport(0, 0, (int)(w * scale.x), (int)(h * scale.y));
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, w, h, 0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();   // and drop raylib's screenScale multiplier (see header)
#endif
}

Viewport::~Viewport() { Shutdown(); }

void Viewport::Init() {
    if (initialised) return;
    lightingShader = LoadLightingShader();
    InitImpostorRenderer();
    orbit.Reset(Vector3Zero(), 20.0f);
    light = CreateLight(LIGHT_DIRECTIONAL, orbit.camera.position, orbit.camera.target, WHITE, lightingShader);
    initialised = true;
    UpdateLighting();
}

void Viewport::Shutdown() {
    if (!initialised) return;
    if (HasTarget()) UnloadRenderTexture(target);
    UnloadShader(lightingShader);
    ShutdownImpostorRenderer();
    width = height = 0;
    initialised = false;
}

void Viewport::Resize(int newWidth, int newHeight) {
    newWidth = newWidth < 1 ? 1 : newWidth;
    newHeight = newHeight < 1 ? 1 : newHeight;
    if (newWidth == width && newHeight == height) return;
    if (HasTarget()) UnloadRenderTexture(target);
    target = LoadRenderTexture(newWidth, newHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    width = newWidth;
    height = newHeight;
}

void Viewport::UpdateLighting() {
    SetShaderValue(lightingShader, lightingShader.locs[SHADER_LOC_VECTOR_VIEW], &orbit.camera.position.x, SHADER_UNIFORM_VEC3);
    light.position = orbit.camera.position;
    light.target = orbit.camera.target;
    UpdateLightValues(lightingShader, light);
}

static void DrawScene(const ViewportScene& scene, const Camera3D& camera) {
    ClearBackground(scene.background);
    BeginMode3D(camera);
    if (scene.model && scene.settings) {
        if (scene.highlighted) scene.model->DrawHighlighted(*scene.highlighted, *scene.settings);
        scene.model->Draw(*scene.settings);
    }
    if (scene.drawGrid) DrawGrid(10, 1.0f);
    EndMode3D();
}

void Viewport::Render(const ViewportScene& scene) {
    if (!HasTarget()) return;
    UpdateLighting();
    BeginTextureMode(target);
    DrawScene(scene, orbit.camera);
    EndTextureMode();
    FixAppleScreenScale();
}

Image Viewport::RenderToImage(const ViewportScene& scene, int imageWidth, int imageHeight, bool transparent) {
    RenderTexture2D rt = LoadRenderTexture(imageWidth, imageHeight);
    UpdateLighting();
    ViewportScene s = scene;
    if (transparent) s.background = BLANK;
    BeginTextureMode(rt);
    DrawScene(s, orbit.camera);
    EndTextureMode();
    FixAppleScreenScale();
    Image image = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&image);
    UnloadRenderTexture(rt);
    return image;
}

Vector2 Viewport::WorldToViewport(Vector3 worldPos) const {
    return GetWorldToScreenEx(worldPos, orbit.camera, width, height);
}

Ray Viewport::ViewportRay(Vector2 viewportPos) const {
    return GetScreenToWorldRayEx(viewportPos, orbit.camera, width, height);
}
