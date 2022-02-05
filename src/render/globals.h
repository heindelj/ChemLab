#pragma once
#define FLT_MAX     340282346638528859811704183484516925440.0f

static float g_ballScale = 0.25;          // scale of balls in ball and stick mode (multiplied by vdw radius)
static float g_stickRadius = 0.2;         // radius of bonds
static bool  g_colorBonds = true;         // For coloring bonds in BALL_AND_STICK mode
static float g_alphaOverlay = 0.4f;       // For overlaying spheres

static Color g_hbondColor = YELLOW;       // Color of hydrogen bonds
static float g_hbondWidth = 3.0f;         // Width of lines for hydrogen bonds
static float g_dashEvery  = 0.15f;        // Length of line before we dash the hydrogen bond
static bool  g_dashedLine = true;         // should h-bonds be dashed lines?
