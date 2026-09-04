#include "app/app_state.h"

#include "graph/graph_system.h"

AppState::AppState() : graphSystem(std::make_unique<graph::GraphSystem>()) {}
AppState::~AppState() = default;

graph::GraphSystem& AppState::GraphSys() { return *graphSystem; }

Structure* AppState::ActiveStructure() {
    if (activeStructure < 0 || activeStructure >= (int)structures.size()) return nullptr;
    return &structures[activeStructure];
}

const Structure* AppState::ActiveStructure() const {
    if (activeStructure < 0 || activeStructure >= (int)structures.size()) return nullptr;
    return &structures[activeStructure];
}

Frames* AppState::ActiveFrames() {
    Structure* s = ActiveStructure();
    return s ? &s->frames : nullptr;
}

ChemicalData* AppState::ActiveChem() {
    Structure* s = ActiveStructure();
    if (!s || s->frames.nframes == 0) return nullptr;
    if (s->activeFrame < 0 || s->activeFrame >= (int)s->frames.nframes) s->activeFrame = 0;
    return &s->frames.data[s->activeFrame];
}

const ChemicalData* AppState::ActiveChem() const {
    const Structure* s = ActiveStructure();
    if (!s || s->frames.nframes == 0) return nullptr;
    const int f = (s->activeFrame < 0 || s->activeFrame >= (int)s->frames.nframes) ? 0 : s->activeFrame;
    return &s->frames.data[f];
}

int AppState::ActiveFrameIndex() const {
    const Structure* s = ActiveStructure();
    return s ? s->activeFrame : -1;
}

int AppState::FrameCount() const {
    const Structure* s = ActiveStructure();
    return s ? (int)s->frames.nframes : 0;
}

bool& AppState::PanelOpen(const char* id) { return panelOpen[id]; }

// ---- 2D plots ----

plot::NamedPlot& AppState::PublishPlot(const std::string& name, plot::PlotSpec spec, bool activate) {
    if (plot::NamedPlot* p = FindPlot(name)) {
        p->spec = std::move(spec);
        ++p->version;
        return *p;
    }
    plot::NamedPlot np;
    np.name = name;
    np.spec = std::move(spec);
    plots.push_back(std::move(np));
    if (activate) twoDPlotIndex = kBuiltinPlotCount + (int)plots.size() - 1;
    return plots.back();
}

plot::NamedPlot* AppState::FindPlot(const std::string& name) {
    for (auto& p : plots)
        if (p.name == name) return &p;
    return nullptr;
}

bool AppState::RemovePlot(const std::string& name) {
    for (size_t i = 0; i < plots.size(); ++i) {
        if (plots[i].name != name) continue;
        const std::string selected = SelectedPlotName();
        plots.erase(plots.begin() + (long)i);
        if (selected == name) twoDPlotIndex = 0;
        else SelectPlot(selected);
        return true;
    }
    return false;
}

void AppState::ClearPlots() {
    plots.clear();
    if (twoDPlotIndex >= kBuiltinPlotCount) twoDPlotIndex = 0;
}

bool AppState::SelectPlot(const std::string& name) {
    if (name == "energy") { twoDPlotIndex = 0; return true; }
    if (name == "measurements") { twoDPlotIndex = 1; return true; }
    for (size_t i = 0; i < plots.size(); ++i)
        if (plots[i].name == name) { twoDPlotIndex = kBuiltinPlotCount + (int)i; return true; }
    return false;
}

std::string AppState::SelectedPlotName() const {
    if (twoDPlotIndex == 0) return "energy";
    if (twoDPlotIndex == 1) return "measurements";
    const int k = twoDPlotIndex - kBuiltinPlotCount;
    if (k >= 0 && k < (int)plots.size()) return plots[k].name;
    return "energy";
}
