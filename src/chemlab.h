#pragma once

// std dependencies
#include <cassert>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <utility>
#include <string>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <stdlib.h>

// external
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "utils/math_utils.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

// core
#include "core/types.h"

//data
#include "render/globals.h"
#include "utils/atomic_data.h"
#include "utils/geometry.h"

#include "utils/load_and_create.h"
#include "core/init.h"

// debug
#include "utils/debug.h"


// utilities, camera, rendering, IO, ... the actual code
#include "render/render_frame.h"
#include "render/camera.h"
#include "utils/interact.h"

// modes
#include "render/view_mode.h"
#include "render/edit_mode.h"
#include "render/animation_mode.h"

// handling things independent of mode
#include "core/frame.h"