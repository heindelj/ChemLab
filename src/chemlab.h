#pragma once
// Library Dependencies
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

// std dependencies
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

// external
#include "raylib.h"
#include "raymath.h"

// core
#include "core/types.h"
#include "core/input.h"

// debug
#include "utils/debug.h"

//data
#include "render/globals.h"
#include "utils/atomic_data.h"
#include "utils/raymath_glm_conversions.h"

// actual code
#include "utils/math_utils.h"
#include "utils/geometry.h"
#include "utils/molecule_tools.h"
#include "render/camera.h"
#include "render/render_frame.h"
