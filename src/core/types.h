enum RenderStyle {
	BALL_AND_STICK,
	STICKS,
	SPHERES
};

enum InteractionMode {
	VIEW,
	EDIT
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

	virtual void Draw() = 0;
	virtual int TestRayAgainst(Ray ray) = 0;
	virtual void free() = 0;
};

struct BallAndStickModel : MolecularModel {
	uint32_t numSpheres;
	uint32_t numSticks;

	Mesh sphereMesh;
	Mesh stickMesh;
	std::vector<Material> materials;

	void Draw() override;
	int  TestRayAgainst(Ray ray) override;	
	void free() {
		UnloadMesh(this->sphereMesh);
		UnloadMesh(this->stickMesh);
		for (auto& material : materials)
			UnloadMaterial(material);
	}
};

struct SpheresModel : MolecularModel {
	uint32_t numSpheres;

	Mesh sphereMesh;
	std::vector<Material> materials;

	void Draw() override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->sphereMesh);
		for (auto& material : materials)
			UnloadMaterial(material);
	}
};

struct SticksModel : MolecularModel {
	uint32_t numSticks;

	Mesh stickMesh;
	std::vector<Material> materials;

	void Draw() override;
	int  TestRayAgainst(Ray ray) override;
	void free() {
		UnloadMesh(this->stickMesh);
		for (auto& material : materials)
			UnloadMaterial(material);
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

struct ActiveContext {
	uint32_t numFrames;
	uint32_t activeFrame;
	Frames* frames;
	MolecularModel* model;

	InteractionMode mode;
	RenderStyle style;
	Camera3D camera;

	double timeOfLastClick;
	float lineWidth;
	std::array<int, 4> viewSelection;
	std::vector<std::array<int, 4>> permanentSelection;
	SelectionStep selectionStep;
};