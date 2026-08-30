#pragma once
#include "raylib.h"

// Per-vertex Phong-ish lighting shader used for every mesh in the scene.
// Sources are embedded so the binary does not depend on the assets folder.
Shader LoadLightingShader();
