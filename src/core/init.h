#pragma once

Camera3D GetCameraWithGoodDefaultPosition(const Frames& frames) {
	Camera3D camera = { 0 };
	camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;
	
	camera.target = centroid(frames.atoms[0].xyz);
	camera.position = camera.target - (Vector3){ 10.0f, 0.0f, 10.0f };
	return camera;
}

ActiveContext InitContext(Frames& frames, const int screenWidth, const int screenHeight) {
	ActiveContext context;
	context.screenWidth = 800;
	context.screenHeight = 450;

	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(context.screenWidth, context.screenHeight, "ChemLab");

	context.mode = VIEW;
	context.style = BALL_AND_STICK;
	context.camera = GetCameraWithGoodDefaultPosition(frames);

	context.frames = &frames;
	context.model = BallAndStickModelFromAtoms(frames.atoms[0]);
	context.activeFrame = 0;
	context.numFrames = frames.nframes;

	context.lineWidth = 3.0f;
	context.timeOfLastClick = 0.0;
	context.viewSelection.fill(-1);
	context.selectionStep = NONE;

	context.isRotating = false;
	context.isCyclingAllFrames = false;
	context.rotationSpeed = 0.2f;

	return context;
}