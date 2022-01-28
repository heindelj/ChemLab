#pragma once

// std dependencies
#include <cassert>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <utility>
#include <string>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

// external
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

// core
#include "core/types.h"
#include "core/input.h"

// debug
#include "utils/debug.h"

//data
#include "render/globals.h"
#include "utils/atomic_data.h"

// utilities, camera, rendering, IO, ... the actual code
#include "utils/math_utils.h"
#include "utils/geometry.h"
#include "utils/load_and_create.h"
#include "render/render_frame.h"
#include "render/camera.h"
#include "utils/interact.h"
#include "utils/export.h"

// modes
#include "render/view_mode.h"
#include "render/edit_mode.h"
#include "render/animation_mode.h"

// handling things independent of mode
#include "core/frame.h"
#include "core/init.h"