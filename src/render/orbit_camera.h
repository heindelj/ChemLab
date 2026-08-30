#pragma once
#include "raylib.h"

// Model-viewer style camera: drag to tumble around the target, scroll to
// zoom, right/middle drag to pan. All inputs are fed in explicitly so the UI
// layer decides when the viewport owns the mouse.
struct OrbitCamera {
    Camera3D camera{};
    float rotateSpeedDegPerPixel = 0.5f;
    float panSpeedPerPixel = 0.01f;   // scaled by distance to target
    float zoomStepFraction = 0.10f;   // fraction of target distance per wheel notch

    void Reset(Vector3 target, float distance);
    void FrameBounds(Vector3 minCorner, Vector3 maxCorner);
    void LookDownAxis(int axis, bool flip);   // 0=x, 1=y, 2=z

    void Rotate(Vector2 mouseDelta);
    void RotateAroundUp(float degrees);        // continuous spin
    void Pan(Vector2 mouseDelta);
    void Zoom(float wheel);

    float TargetDistance() const;
    Vector3 Forward() const;
    Vector3 Right() const;
};
