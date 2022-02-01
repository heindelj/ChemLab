enum RenderStyle {
	BALL_AND_STICK,
	STICKS,
	SPHERES
};

enum InteractionMode {
	VIEW = 0,
	EDIT = 1,
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

// Having a base MolecularModel class makes it easier to do things like hit detection
// and geometry editing in a manner that is independent of the style with which we draw the molecule.
struct MolecularModel
{
	std::vector<Matrix> transforms;
	std::vector<Material> materials;

	virtual void Draw() = 0;
	virtual int TestRayAgainst(Ray ray) = 0;
	virtual void free() = 0;
};

struct BallAndStickModel : MolecularModel {
	uint32_t numSpheres;
	uint32_t numSticks;

	Mesh sphereMesh;
	Mesh stickMesh;

	void Draw() override;
	int  TestRayAgainst(Ray ray) override;	
	void free() {
		UnloadMesh(this->sphereMesh);
		UnloadMesh(this->stickMesh);
	}
};

struct SpheresModel : MolecularModel {
	uint32_t numSpheres;

	Mesh sphereMesh;

	void Draw() override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->sphereMesh);
	}
};

struct SticksModel : MolecularModel {
	uint32_t numSticks;

	Mesh stickMesh;

	void Draw() override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->stickMesh);
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
};

struct RenderContext {
	Camera3D camera;
	Shader lightingShader;
	MolecularModel* model;
	Light light;
};

struct ActiveContext {
	int screenWidth;
    int screenHeight;

    std::thread computeThread;

	uint32_t numFrames;
	uint32_t activeFrame;
	Frames* frames;
	
	RenderContext renderContext;

	InteractionMode mode;
	RenderStyle style;

	// UI Settings
	bool modeDropdownEdit;

	bool drawGrid;

	// view mode context variables
	double timeOfLastClick;
	float lineWidth;
	std::array<int, 4> viewSelection;
	std::vector<std::array<int, 4>> permanentSelection;
	SelectionStep selectionStep;

	Vector3 forwardOnStartingToRotate;
	float rotationSpeed;
	bool isRotating;
	bool isCyclingAllFrames;

	bool exportRotation;
	bool exportAllFrames;
};