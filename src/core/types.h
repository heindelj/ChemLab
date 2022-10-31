
enum RenderStyle {
	BALL_AND_STICK,
	STICKS,
	SPHERES
};

enum InteractionMode {
	VIEW = 0,
	BUILD = 1,
	ANIMATION = 2,
	NUMBER_OF_MODES = 3 
	// NOTE THAT THIS ^^^ IS THE NUMBER OF VALID MODES 
	// AND IS USED TO WRAP AROUND TO THE END. IT MUST BE THE LAST ELEMENT IN THE ENUM
};

enum SelectionStep {
	NONE,
	DISTANCE,
	ANGLE,
	DIHEDRAL
};

struct Axes3D {
	Mesh cylinderMeshX;
	Mesh coneMeshX;
	Mesh cylinderMeshY;
	Mesh coneMeshY;
	Mesh cylinderMeshZ;
	Mesh coneMeshZ;

	Material materialX;
	Material materialY;
	Material materialZ;

	Matrix transformX;
	Matrix transformY;
	Matrix transformZ;


	void Draw(const Vector3& point) {
		// Y axis
		transformY = MatrixTranslate(point);
		DrawMesh(cylinderMeshY, materialY, transformY);
		DrawMesh(coneMeshY, materialY, transformY * MatrixTranslate((Vector3){0.0f, 1.0f, 0.0f}));
		// Z axis
		transformZ = MatrixAlignToAxis((Vector3){0,1,0}, (Vector3){0,0,1}) * MatrixTranslate(point);
		DrawMesh(cylinderMeshZ, materialZ, transformZ);
		DrawMesh(coneMeshZ, materialZ, transformZ * MatrixTranslate((Vector3){0.0f, 0.0f, 1.0f}));
		// X axis
		transformX = MatrixAlignToAxis((Vector3){0,1,0}, (Vector3){1,0,0}) * MatrixTranslate(point);
		DrawMesh(cylinderMeshX, materialX, transformX);
		DrawMesh(coneMeshX, materialX, transformX * MatrixTranslate((Vector3){1.0f, 0.0f, 0.0f}));
	}

	int TestRayAgainst(Ray ray) {
		int collisonIndex = -1; // default which is returned on no collision
		float collisionDistance = FLT_MAX;

		// SPEED: This may be very slow for large numbers of atoms
		// so we should consider using a proper data structure
		// for this if it becomes a bottleneck
		RayCollision collisionInfo[6];
		collisionInfo[0] = GetRayCollisionMesh(ray, this->cylinderMeshX, this->transformX);
		collisionInfo[1] = GetRayCollisionMesh(ray, this->coneMeshX, this->transformX * MatrixTranslate((Vector3){1.0f, 0.0f, 0.0f}));
		collisionInfo[2] = GetRayCollisionMesh(ray, this->cylinderMeshY, this->transformY);
		collisionInfo[3] = GetRayCollisionMesh(ray, this->coneMeshY, this->transformY * MatrixTranslate((Vector3){0.0f, 1.0f, 0.0f}));
		collisionInfo[4] = GetRayCollisionMesh(ray, this->cylinderMeshZ, this->transformZ);
		collisionInfo[5] = GetRayCollisionMesh(ray, this->coneMeshZ, this->transformZ * MatrixTranslate((Vector3){0.0f, 0.0f, 1.0f}));
		// Check if we got a collision and then make sure we don't
		// overwrite a collision with a closer object
		for (int i = 0; i < 6; ++i) {
			if (collisionInfo[i].hit) {
				if (collisonIndex == -1) {
					if (i == 0 || i == 1)
						collisonIndex = 0; // Hit x axis
					if (i == 2 || i == 3)
						collisonIndex = 1; // Hit y axis
					if (i == 4 || i == 5)
						collisonIndex = 2; // Hit z axis
					
					collisionDistance = collisionInfo[i].distance;
				} else if (collisionInfo[i].distance < collisionDistance) {
					if (i == 0 || i == 1)
						collisonIndex = 0; // Hit x axis
					if (i == 2 || i == 3)
						collisonIndex = 1; // Hit y axis
					if (i == 4 || i == 5)
						collisonIndex = 2; // Hit z axis

					collisionDistance = collisionInfo[i].distance;
				}		
			}
		}
		return collisonIndex;
	}

	Axes3D(Shader* shader) {
		cylinderMeshX = GenMeshCylinder(0.1, 1.0, 24);
		coneMeshX = GenMeshCone(0.2, 0.25, 24);
		cylinderMeshY = GenMeshCylinder(0.1, 1.0, 24);
		coneMeshY = GenMeshCone(0.2, 0.25, 24);
		cylinderMeshZ = GenMeshCylinder(0.1, 1.0, 24);
		coneMeshZ = GenMeshCone(0.2, 0.25, 24);
		
		materialX = LoadMaterialDefault();
		materialX.shader = *shader;
		materialX.maps[MATERIAL_MAP_DIFFUSE].color = RED;

		materialY = LoadMaterialDefault();
		materialY.shader = *shader;
		materialY.maps[MATERIAL_MAP_DIFFUSE].color = GREEN;

		materialZ = LoadMaterialDefault();
		materialZ.shader = *shader;
		materialZ.maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
	}
};

struct ZMatrix {
	Matrix transform; // encodes centroid position and rotational orientation of all atoms
	std::vector<std::array<float,3>> coordinates; // stores distance (angstrom), angle (radians), and dihedral (radians)
	std::vector<std::array<int, 3>> coordinateIndices;
	// ^^^^ MUST BE THE SAME LENGTH AS COORDINATES ^^^^
	// stores which atom the coordinates are relative to. 
	// e.g. 1,2,3 would mean the distance is relative to atom index 1, angle to atom index 2, etc.
};


struct RenderingIndices {
	int materialIndex;
	int firstTransformIndex;
	int numTransforms;
};

struct EntityIndices {
	int meshIndex;
	int materialIndex;
	int transformIndex;
};

// Having a base MolecularModel class makes it easier to do things like hit detection
// and geometry editing in a manner that is independent of the style with which we draw the molecule.
struct MolecularModel
{
	Matrix modelTransform; // a transform applied to every mesh in the model uniformly. Used for translating and scaling an entire model.
	std::vector<std::vector<Matrix>> transforms; // array of size of materials with each transform which has that material
	std::vector<Material> materials;
	std::map<std::string, int> labelToMaterialIndex; // maps atom labels to material index in the materials vector
	std::vector<std::vector<int>> stickIndices; // indices of material for each stick for a given sphere index. Can have empty lists. Only used for ball and stick currently.
	std::map<int, EntityIndices> IDToEntityIndices; // maps any entity (atom, stick, etc.) to its corresponding material and transform indices
	std::map<std::pair<int, int>, int> stickMaterialAndTransformToID; // maps the material and trasnform indices of a stick to its entity ID.

	// Takes an index for a mesh and returns the material index
	// and first transform index for this material and the number of those transforms.
	std::map<int, std::vector<RenderingIndices>> meshIndexToRenderingIndices;
	std::vector<Mesh> meshes; // all meshes to be drawn

	virtual void Draw() = 0;
	virtual void DrawHighlighted(const std::set<int>& indices) = 0;
	virtual int TestRayAgainst(Ray ray) = 0;
	virtual void free() = 0;
};

struct BallAndStickModel : MolecularModel {
	uint32_t numSpheres;
	uint32_t numSticks;

	Mesh sphereMesh;
	Mesh stickMesh;

	void Draw() override;
	void DrawHighlighted(const std::set<int>& indices) override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->sphereMesh);
		UnloadMesh(this->stickMesh);
		delete this;
	}
};

struct SpheresModel : MolecularModel {
	uint32_t numSpheres;

	Mesh sphereMesh;

	void Draw() override;
	void DrawHighlighted(const std::set<int>& indices) override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->sphereMesh);
		delete this;
	}
};

struct SticksModel : MolecularModel {
	uint32_t numSticks;

	Mesh stickMesh;

	void Draw() override;
	void DrawHighlighted(const std::set<int>& indices) override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->stickMesh);
		delete this;
	}
};

struct RenderData {
	Color color;
	float vdwRadius;
	float covalentRadius;
};

struct BondList {
	std::vector<std::pair<uint32_t, uint32_t>> pairs;
};

struct Atoms {
	uint32_t natoms;
	std::vector<Vector3> xyz;
	std::vector<std::string> labels;
	BondList covalentBondList;
	std::vector<RenderData> renderData;
};

struct Frames {
	uint32_t nframes;
	std::vector<Atoms> atoms;
	std::vector<std::string> headers;
	std::unordered_map<std::string, std::filesystem::file_time_type> loadedFiles; // dictionary to store file paths and last modification time of file
};

struct UISettings {
	float borderWidth;
	float menuWidth;
	Color colorPickerValue;
	float colorPickerAlpha;
};

struct Window {
	int modelID; // The frame number that will be drawn into this window.
	Rectangle rect;
};

struct RenderContext {
	Camera3D camera;
	int windowID;
	RenderTexture2D renderTarget;

	Color backgroundColor;
	
	Shader outlineShader;
	Shader lightingShader;

	MolecularModel* model;
	Light light;
};

struct ActiveContext {
	int screenWidth;
    int screenHeight;

    std::vector<Window> windows;

    std::vector<std::thread> computeThreads;

    // Frame variables
	uint32_t activeFrame;
	Frames* frames;
	
	RenderContext renderContext;
	UISettings uiSettings;

	InteractionMode mode;
	RenderStyle style;

	// UI Settings
	bool drawUI;
	bool drawGrid;
	bool lockCamera;
	bool drawNumbers;

	// view mode context variables
	double timeOfLastClick;
	float lineWidth;
	std::set<int> atomsToHighlight;
	std::array<int, 4> viewSelection; // selected entities by ID
	std::vector<std::array<int, 4>> permanentSelection; // selected entities by ID
	SelectionStep selectionStep;
	Vector3 forwardOnStartingToRotate;
	float rotationSpeed;
	bool isRotating;
	bool isCyclingAllFrames;
	bool monitorFileChanges;
	float timeBetweenFrameChanges;
	double timeOfLastFrameChange;

	// build mode variables
	bool addingNewAtoms;
	std::vector<int> indicesBeingAdded;
	bool buildingZMatrix;
	ZMatrix* zmat;
	bool spinnerEditMode;
	int distanceIndex;
	int angleIndex;
	int dihedralIndex;
	float distanceSliderValue;
	float angleSliderValue;
	float dihedralSliderValue;

	bool isAxisSelected;
	int activeAxis;
	Vector3 lastPoint;

	// Animation mode variables
	bool exportRotation;
	bool exportAllFrames;
	bool takeScreenshot;

	// miscellaneous
	Model gridModel;
	Axes3D* axes;
};