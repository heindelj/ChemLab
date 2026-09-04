#pragma once
// Per-element rendering data, looked up by atomic number: CPK colour, van der
// Waals radius (sphere size) and covalent radius (bond perception). Elements
// without a table entry get a fallback (magenta, 1.8 A, no bonds) so drawing
// never fails; HasElementData() says whether a Z is fully tabulated.

#include <cstdint>

#include "raylib.h"

Color ElementColor(int32_t z);
float VdwRadius(int32_t z);
float CovalentRadius(int32_t z);
bool HasElementData(int32_t z);
