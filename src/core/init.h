#pragma once

Camera3D GetCameraWithGoodDefaultPosition(const std::vector<Vector3>& xyz) {
	Camera3D camera = { 0 };
	camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;
	
	camera.target = centroid(xyz);
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
	context.camera = GetCameraWithGoodDefaultPosition(frames.atoms[0].xyz);

	context.drawGrid = true;
	
	// Create material that is shared by all models
	Shader lightingShader = LoadShader("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
	lightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightingShader, "viewPos");
	// Ambient light level (some basic lighting)
	int ambientLoc = GetShaderLocation(lightingShader, "ambient");
	Vector4 ambient = (Vector4){ 0.2f, 0.2f, 0.2f, 1.0f };
	SetShaderValue(lightingShader, ambientLoc, &ambient.x, SHADER_UNIFORM_VEC4);
	context.lightingShader = lightingShader;

	context.frames = &frames;
	context.model = MolecularModelFromAtoms(frames.atoms[0], &context.lightingShader, BALL_AND_STICK);
	context.light = CreateLight(LIGHT_DIRECTIONAL, context.camera.position, context.camera.target, WHITE, context.lightingShader);
	context.activeFrame = 0;
	context.numFrames = frames.nframes;

	context.lineWidth = 3.0f;
	context.timeOfLastClick = 0.0;
	context.viewSelection.fill(-1);
	context.selectionStep = NONE;

	float xPosScale = ((float)(context.screenWidth - 40) - 55.0f) / context.screenWidth;
	context.isRotating = false;
	context.isCyclingAllFrames = false;
	context.rotationSpeed = 0.2f;

	context.exportRotation = false;
	context.exportAllFrames = false;

	return context;
}