#include "render/orbit_camera.h"

#include <cmath>

#include "raymath.h"

#include "core/math_utils.h"

void OrbitCamera::Reset(Vector3 target, float distance) {
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.target = target;
    camera.position = target + Vector3{0.0f, 0.0f, distance};
}

void OrbitCamera::FrameBounds(Vector3 minCorner, Vector3 maxCorner) {
    const Vector3 center = 0.5f * (minCorner + maxCorner);
    const float radius = std::fmax(0.5f * norm(maxCorner - minCorner), 1.0f);
    // Fit the bounding sphere into the vertical field of view with some margin.
    const float distance = radius / std::sin(0.5f * camera.fovy * DEG2RAD) * 1.15f;
    Reset(center, distance);
}

void OrbitCamera::LookDownAxis(int axis, bool flip) {
    const float d = TargetDistance();
    Vector3 dir, up;
    switch (axis) {
        case 0: dir = Vector3{1, 0, 0}; up = Vector3{0, 1, 0}; break;
        case 1: dir = Vector3{0, 1, 0}; up = Vector3{0, 0, -1}; break;
        default: dir = Vector3{0, 0, 1}; up = Vector3{0, 1, 0}; break;
    }
    if (flip) dir = -1.0f * dir;
    camera.position = camera.target + d * dir;
    camera.up = up;
}

float OrbitCamera::TargetDistance() const { return norm(camera.target - camera.position); }
Vector3 OrbitCamera::Forward() const { return normalize(camera.target - camera.position); }
Vector3 OrbitCamera::Right() const { return normalize(cross(Forward(), camera.up)); }

void OrbitCamera::Rotate(Vector2 mouseDelta) {
    if (mouseDelta.x == 0.0f && mouseDelta.y == 0.0f) return;
    // Turntable-free tumble: rotate about the screen-space axis perpendicular
    // to the drag, pivoting around the target. Same maths as the original
    // ChemLab rotateAroundTargetView, without the mouse-delta spike hack.
    const Matrix V = MatrixLookAt(camera.position, camera.target, camera.up);
    const Vector3 pivot = ToVector3(V * Vector4{camera.target.x, camera.target.y, camera.target.z, 1.0f});
    const Vector3 axis = Vector3{mouseDelta.y, mouseDelta.x, 0.0f};
    const float angle = norm(mouseDelta) * rotateSpeedDegPerPixel * DEG2RAD;
    const Matrix R = MatrixRotate(normalize(axis), angle);
    const Matrix RP = MatrixTranslate(-1.0f * pivot) * R * MatrixTranslate(pivot);
    const Matrix C = MatrixInvert(V * RP);

    const float d = TargetDistance();
    camera.position = Vector3{C.m12, C.m13, C.m14};
    camera.target = camera.position - d * Vector3{C.m8, C.m9, C.m10};
    camera.up = normalize(Vector3{C.m4, C.m5, C.m6});
}

void OrbitCamera::RotateAroundUp(float degrees) {
    const Vector3 offset = camera.position - camera.target;
    const Matrix R = MatrixRotate(camera.up, degrees * DEG2RAD);
    camera.position = camera.target + ToVector3(R * ToVector4(offset));
}

void OrbitCamera::Pan(Vector2 mouseDelta) {
    const float scale = panSpeedPerPixel * std::fmax(TargetDistance(), 1.0f) * 0.1f;
    const Vector3 right = Right();
    const Vector3 up = normalize(cross(right, Forward()));
    const Vector3 shift = (-mouseDelta.x * scale) * right + (mouseDelta.y * scale) * up;
    camera.position += shift;
    camera.target += shift;
}

void OrbitCamera::Zoom(float wheel) {
    if (wheel == 0.0f) return;
    const float d = TargetDistance();
    const float newD = std::fmax(0.5f, d * (1.0f - zoomStepFraction * wheel));
    camera.position = camera.target - newD * Forward();
}
