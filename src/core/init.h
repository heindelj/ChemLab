#pragma once

Camera3D GetCameraWithGoodDefaultPosition(const std::vector<Vector3>& xyz) {
	Camera3D camera = { 0 };
	camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;
	
	camera.target = centroid(xyz);
	camera.position = camera.target - (Vector3){ 0.0f, 0.0f, 15.0f };
	return camera;
}

Shader InitOutlineShader(int screenWidth, int screenHeight) {
	///////// Outline Shader variables, etc. ///////////
    Shader shdrOutline = LoadShader(0, "assets/shaders/outline.fs");

    float outlineSize = 2.0f;
    float outlineColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };     // Normalized YELLOW color 
    float textureSize[2] = { (float)screenWidth, (float)screenHeight };
	    
    // Get shader locations
    int outlineSizeLoc = GetShaderLocation(shdrOutline, "outlineSize");
    int outlineColorLoc = GetShaderLocation(shdrOutline, "outlineColor");
    int textureSizeLoc = GetShaderLocation(shdrOutline, "textureSize");
	    
    // Set shader values (they can be changed later)
    SetShaderValue(shdrOutline, outlineSizeLoc, &outlineSize, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shdrOutline, outlineColorLoc, outlineColor, SHADER_UNIFORM_VEC4);
    SetShaderValue(shdrOutline, textureSizeLoc, textureSize, SHADER_UNIFORM_VEC2);
    return shdrOutline;
}

Shader InitLightingShader() {
	Shader lightingShader = LoadShader("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
	lightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightingShader, "viewPos");
	int ambientLoc = GetShaderLocation(lightingShader, "ambient");
	
	Vector4 ambient = (Vector4){ 0.2f, 0.2f, 0.2f, 1.0f };

	SetShaderValue(lightingShader, ambientLoc, &ambient.x, SHADER_UNIFORM_VEC4);
	return lightingShader;
}

RenderContext InitRenderContext(const Atoms& atoms, Window* window) {
	RenderContext renderContext;

	renderContext.camera = GetCameraWithGoodDefaultPosition(atoms.xyz);
	renderContext.windowID = 0;
	renderContext.backgroundColor = Color(30, 30, 30, 255);

	renderContext.outlineShader = InitOutlineShader(window->rect.width, window->rect.height);
	renderContext.renderTarget  = LoadRenderTexture(window->rect.width, window->rect.height);

	renderContext.lightingShader = InitLightingShader();
	renderContext.model = MolecularModelFromAtoms(atoms, &renderContext.lightingShader, BALL_AND_STICK);
	renderContext.light = CreateLight(LIGHT_DIRECTIONAL, renderContext.camera.position, renderContext.camera.target, WHITE, renderContext.lightingShader);

	return renderContext;
}

ActiveContext InitContext(Frames& frames, const int screenWidth, const int screenHeight) {
	ActiveContext context;
	context.frames = &frames;

	context.screenWidth  = screenWidth;
	context.screenHeight = screenHeight;

	context.windows.push_back((Window){0, (Rectangle){0, 0, (float)screenWidth, (float)screenHeight}});

	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(context.screenWidth, context.screenHeight, "ChemLab");
	SetTargetFPS(60);

	context.mode = VIEW;
	context.style = BALL_AND_STICK;

	context.uiSettings = (UISettings){5.0f, screenWidth / 5.0f - 10.0f, BLUE, 1.0f};
	context.renderContext = InitRenderContext(frames.atoms[0], &context.windows[0]);

	// UI Settings
	context.drawUI = true;
	context.drawGrid = true;
	context.lockCamera = false;
	
	context.activeFrame = 0;

	context.lineWidth = 3.0f;
	context.timeOfLastClick = 0.0;
	context.viewSelection.fill(-1);
	context.selectionStep = NONE;

	float xPosScale = ((float)(context.screenWidth - 40) - 55.0f) / context.screenWidth;
	// view mode
	context.isRotating = false;
	context.isCyclingAllFrames = false;
	context.monitorFileChanges = false;
	context.rotationSpeed = 0.2f;
	context.timeBetweenFrameChanges = 0.5f;
	context.timeOfLastFrameChange = 0.0;

	context.exportRotation = false;
	context.exportAllFrames = false;
	context.takeScreenshot = false;

	// build mode
	context.addingNewAtoms = false;

	// miscellaneous
	context.gridModel = LoadModelFromMesh(GenMeshPlane(10, 10, 10, 10));

	return context;
}