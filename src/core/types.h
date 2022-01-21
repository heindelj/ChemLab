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

struct ActiveContext {
	InteractionMode mode;
	RenderStyle style;
	Camera3D camera;

	double timeOfLastClick;
	std::array<int, 4> viewSelection;
	std::vector<std::array<int, 4>> permanentSelection;
	SelectionStep selectionStep;
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