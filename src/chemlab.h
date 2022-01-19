#pragma once
// Library Dependencies
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

// std dependencies
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

// my includes
#include "raylib.h"
#include "raymath.h"
#include "render/globals.h"
#include "utils/atomic_data.h"
#include "utils/raymath_glm_conversions.h"
#include "utils/molecule_tools.h"
#include "utils/atomic_data.h"
#include "utils/geometry.h"
#include "render/camera.h"
#include "render/render_frame.h"
#include "utils/debug.h"
