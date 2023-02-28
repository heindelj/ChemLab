#pragma once

Camera3D GetCameraWithGoodDefaultPosition(const std::vector<Vector3>& xyz) {
	Camera3D camera = { 0 };
	camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;
	
	camera.target = centroid(xyz);
	camera.position = camera.target - (Vector3){ 0.0f, 0.0f, 20.0f };
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
	const char* lighting_fs = 
	"#version 330\n"
	"// Input vertex attributes (from vertex shader)\n"
	"in vec3 fragPosition;\n"
	"in vec2 fragTexCoord;\n"
	"in vec4 fragColor;\n"
	"in vec3 fragNormal;\n"
	"// Output fragment color\n"
	"out vec4 finalColor;\n"
	"#define     MAX_LIGHTS              4\n"
	"#define     LIGHT_DIRECTIONAL       0\n"
	"#define     LIGHT_POINT             1\n"
	"struct MaterialProperty {\n"
	"    vec3 color;\n"
	"    int useSampler;\n"
	"    sampler2D sampler;\n"
	"};\n"
	"struct Light {\n"
	"	int enabled;\n"
	"	int type;\n"
	"	vec3 position;\n"
	"	vec3 target;\n"
	"	vec4 color;\n"
	"};\n"
	"// Input uniform values\n"
	"uniform sampler2D texture0;\n"
	"uniform vec4 colDiffuse;\n"
	"// Input lighting values\n"
	"uniform Light lights[MAX_LIGHTS];\n"
	"uniform vec4 ambient;\n"
	"uniform vec3 viewPos;\n"
	"void main()\n"
	"{\n"
	    "// Texel color fetching from texture sampler\n"
	    "vec4 texelColor = texture(texture0, fragTexCoord);\n"
	    "vec3 lightDot = vec3(0.0);\n"
	    "vec3 normal = normalize(fragNormal);\n"
	    "vec3 viewD = normalize(viewPos - fragPosition);\n"
	    "vec3 specular = vec3(0.0);\n"
	    "for (int i = 0; i < MAX_LIGHTS; i++)\n"
	    "{\n"
	        "if (lights[i].enabled == 1)\n"
	         "{\n"
	            "vec3 light = vec3(0.0);\n"
	    		"\n"
	            "if (lights[i].type == LIGHT_DIRECTIONAL)\n"
	            "{\n"
	            "    light = -normalize(lights[i].target - lights[i].position);\n"
	            "}\n"
	            "\n"
	            "if (lights[i].type == LIGHT_POINT)\n"
	            "{\n"
	            "    light = normalize(lights[i].position - fragPosition);\n"
	            "}\n"
	            "\n"
	            "float NdotL = max(dot(normal, light), 0.0);\n"
	            "lightDot += lights[i].color.rgb*NdotL;\n"
	            "\n"
	            "float specCo = 0.0;\n"
	            "if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 16.0); // 16 refers to shine\n"
	            "specular += specCo / 10; // I am scaling down by 10 because the specular is too intense. Just turning it off looks fine honestly\n"
	    	"}\n"
	    "}\n"
	    "finalColor = (texelColor*((colDiffuse + vec4(specular, 1.0))*vec4(lightDot, 1.0)));\n"
	    "finalColor += texelColor*ambient*colDiffuse;\n"
	    "// Gamma correction\n"
	    "finalColor = pow(finalColor, vec4(1.0/2.2));\n"
	    "finalColor.a = colDiffuse.a;\n"
	"}\n";

	const char* lighting_vs = 
	"#version 330\n"
	"// Input vertex attributes\n"
	"in vec3 vertexPosition;\n"
	"in vec2 vertexTexCoord;\n"
	"in vec3 vertexNormal;\n"
	"in vec4 vertexColor;\n"
	"// Input uniform values\n"
	"uniform mat4 mvp;\n"
	"uniform mat4 matModel;\n"
	"uniform mat4 matNormal;\n"
	"// Output vertex attributes (to fragment shader)\n"
	"out vec3 fragPosition;\n"
	"out vec2 fragTexCoord;\n"
	"out vec4 fragColor;\n"
	"out vec3 fragNormal;\n"
	"void main()\n"
	"{\n"
	"    // Send vertex attributes to fragment shader\n"
	"    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));\n"
	"    fragTexCoord = vertexTexCoord;\n"
	"    fragColor = vertexColor;\n"
	"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));\n"
	"    // Calculate final vertex position\n"
	"    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
	"}\n";

	const char* lighting_instanced_vs = 
	"#version 330\n"
	"// Input vertex attributes\n"
	"in vec3 vertexPosition;\n"
	"in vec2 vertexTexCoord;\n"
	"in vec3 vertexNormal;\n"
	"//in vec4 vertexColor;      // Not required\n"
	"in mat4 instanceTransform;\n"
	"// Input uniform values\n"
	"uniform mat4 mvp;\n"
	"uniform mat4 matModel;\n"
	"uniform mat4 matNormal;\n"
	"// Output vertex attributes (to fragment shader)\n"
	"out vec3 fragPosition;\n"
	"out vec2 fragTexCoord;\n"
	"out vec4 fragColor;\n"
	"out vec3 fragNormal;\n"
	"// NOTE: Add here your custom variables\n"
	"void main()\n"
	"{\n"
	"    // Compute MVP for current instance\n"
	"    mat4 mvpi = mvp*instanceTransform;\n"
	"    // Send vertex attributes to fragment shader\n"
	"    fragPosition = vec3(mvpi*vec4(vertexPosition, 1.0));\n"
	"    fragTexCoord = vertexTexCoord;\n"
	"    //fragColor = vertexColor;\n"
	"    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));\n"
	"    // Calculate final vertex position\n"
	"    gl_Position = mvpi*vec4(vertexPosition, 1.0);\n"
	"}\n";
	Shader lightingShader = LoadShaderFromMemory(lighting_vs, lighting_fs);
	//Shader lightingShader = LoadShaderFromMemory(lighting_instanced_vs, lighting_fs);
	lightingShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(lightingShader, "mvp");
	lightingShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightingShader, "viewPos");
	lightingShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(lightingShader, "instanceTransform");
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

	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(context.screenWidth, context.screenHeight, "ChemLab");
	SetTargetFPS(60);

	context.mode = VIEW;
	context.style = BALL_AND_STICK;

	context.uiSettings = (UISettings){5.0f, screenWidth / 5.0f - 10.0f, BLUE, 1.0f};
	context.renderContext = InitRenderContext(frames.atoms[0], &context.windows[0]);

	// UI Settings
	context.drawUI = false;
	context.drawGrid = true;
	context.drawNumbers = false;
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
	context.buildingZMatrix = false;
	context.spinnerEditMode = false;
	context.distanceIndex = 0;
	context.angleIndex = 0;
	context.dihedralIndex = 0;
	context.distanceSliderValue = 1.0f;
	context.angleSliderValue = 0.0f;
	context.dihedralSliderValue = 0.0f;
	context.isAxisSelected = false;
	context.activeAxis = -1;
	context.lastPoint = (Vector3){0.0f, 0.0f, 0.0f};

	// miscellaneous
	context.gridModel = LoadModelFromMesh(GenMeshPlane(10, 10, 10, 10));
	context.axes = new Axes3D(&context.renderContext.lightingShader);

	return context;
}
