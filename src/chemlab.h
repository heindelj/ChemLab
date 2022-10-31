#pragma once

// std dependencies
#include <cassert>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <utility>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <stdlib.h>
#include <cmath>
#include <fmt/format.h>

// external
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"
#include "utils/math_utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// core
#include "render/globals.h"
#include "core/types.h"

//data
#include "utils/atomic_data.h"
#include "utils/geometry.h"

#include "utils/load_and_create.h"
#include "utils/monitor_file.h"
#include "core/init.h"

// debug
#include "utils/debug.h"

// external
#include "external/tinyfiledialogs.h"
#include "external/tinyfiledialogs.c"

// utilities, camera, rendering, IO, ... the actual code
#include "render/drawing_window.h"
#include "render/draw.h"
#include "render/camera.h"
#include "utils/interact.h"

// modes
#include "core/change_state.h"
#include "render/view_mode.h"
#include "render/build_mode.h"
#include "render/animation_mode.h"

// handling things independent of mode
#include "render/ui.h"
#include "core/frame.h"
