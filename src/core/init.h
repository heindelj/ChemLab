#pragma once

ActiveContext InitContext(Frames& frames, const int screenWidth, const int screenHeight) {
	ActiveContext context;
	context.screenWidth = 800;
	context.screenHeight = 450;

	// Define the camera to look into our 3d world
	Camera3D camera = { 0 };
	camera.position = (Vector3){ -10.0f, 15.0f, -10.0f };   // Camera position
	camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };          // Camera looking at point
	camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };              // Camera up vector (rotation towards target)
	camera.fovy = 45.0f;                                    // Camera field-of-view Y
	camera.projection = CAMERA_PERSPECTIVE;                 // Camera mode type

	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(context.screenWidth, context.screenHeight, "ChemLab");

	context.mode = VIEW;
	context.style = BALL_AND_STICK;
	context.camera = camera;
	context.lineWidth = 3.0f;
	context.timeOfLastClick = 0.0;
	context.viewSelection.fill(-1);
	context.selectionStep = NONE;
	context.frames = &frames;
	context.model = BallAndStickModelFromAtoms(frames.atoms[0]);
	context.activeFrame = 0;
	context.numFrames = frames.nframes;

	return context;
}