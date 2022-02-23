
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

struct ZMatrix {
	Matrix transform; // encodes centroid position and rotational orientation of all atoms
	std::vector<std::array<float,3>> coordinates; // stores distance (angstrom), angle (radians), and dihedral (radians)
	std::vector<std::array<int, 3>> coordinateIndices;
	// ^^^^ MUST BE THE SAME LENGTH AS COORDINATES ^^^^
	// stores which atom the coordinates are relative to. 
	// e.g. 1,2,3 would mean the distance is relative to atom index 1, angle to atom index 2, etc.
};

// Having a base MolecularModel class makes it easier to do things like hit detection
// and geometry editing in a manner that is independent of the style with which we draw the molecule.
struct MolecularModel
{
	Matrix modelTransform; // a transform applied to every mesh in the model uniformly. Used for translating and scaling an entire model.
	std::vector<Matrix> transforms; // Note that atom indices and transform indices are and MUST be the same.
	std::vector<Material> materials;
	std::vector<std::vector<int>> stickIndices; // indices of material for each stick for a given sphere index. Can have empty lists. Only used for ball and stick currently.

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

	// view mode context variables
	double timeOfLastClick;
	float lineWidth;
	std::set<int> atomsToHighlight;
	std::array<int, 4> viewSelection;
	std::vector<std::array<int, 4>> permanentSelection;
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
	int spinnerValue1;
	int spinnerValue2;
	int spinnerValue3;
	float distanceSliderValue;
	float angleSliderValue;
	float dihedralSliderValue;

	// Animation mode variables
	bool exportRotation;
	bool exportAllFrames;
	bool takeScreenshot;

	// miscellaneous
	Model gridModel;
};