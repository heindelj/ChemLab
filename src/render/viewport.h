#pragma once
// A 3D viewport rendered into an off-screen texture so it can be shown as an
// image inside a docked ImGui window. The viewport owns the lighting shader,
// the light and the GPU model for the frame being shown.

#include <set>

#include "raylib.h"
#include "rlights.h"

#include "render/molecular_model.h"
#include "render/orbit_camera.h"

struct ViewportScene {
    const MolecularModel* model = nullptr;
    const std::set<int>* highlighted = nullptr;
    const RenderSettings* settings = nullptr;
    bool drawGrid = true;
    Color background = Color{30, 30, 30, 255};
};

// raylib 5.5 on macOS: GLFW's content-scale callback fires when a Retina
// window first appears and raylib then multiplies every 2D draw by the DPI
// scale, even though its viewport/ortho already account for it. The result is
// a UI drawn at 2x while the mouse stays in logical points ("mouse offset").
// Calling this right after BeginDrawing()/EndTextureMode() restores the
// identity modelview those functions would otherwise scale. No-op elsewhere.
void FixAppleScreenScale();

class Viewport {
public:
    Viewport() = default;
    ~Viewport();
    Viewport(const Viewport&) = delete;
    Viewport& operator=(const Viewport&) = delete;

    void Init();
    void Shutdown();

    // Ensure the render target matches the requested pixel size.
    void Resize(int width, int height);
    int Width() const { return width; }
    int Height() const { return height; }
    const RenderTexture2D& Target() const { return target; }
    bool HasTarget() const { return width > 0 && height > 0; }

    // Draw the scene into the render target.
    void Render(const ViewportScene& scene);
    // Draw the scene into a fresh image of the given size (for screenshots).
    Image RenderToImage(const ViewportScene& scene, int imageWidth, int imageHeight, bool transparent);

    // Coordinate helpers in viewport pixel space (origin top-left of the image).
    Vector2 WorldToViewport(Vector3 worldPos) const;
    Ray ViewportRay(Vector2 viewportPos) const;

    Shader LightingShader() const { return lightingShader; }
    void UpdateLighting();

    OrbitCamera orbit;

private:
    bool initialised = false;
    int width = 0, height = 0;
    RenderTexture2D target{};
    Shader lightingShader{};
    Light light{};
};
