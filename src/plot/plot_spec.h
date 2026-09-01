#pragma once
// plot::PlotSpec -- a UI-free description of a 2D plot: a title, axis labels
// and a list of series, each with a kind (line, scatter, bars, ...) and its
// data. Nodes build these and publish them under a name (AppState::plots);
// the 2D Plot panel renders whichever named plot is selected. Nothing here
// touches ImPlot, so the description can be built anywhere (scripts, tests).

#include <cstdint>
#include <string>
#include <vector>

namespace plot {

enum class SeriesKind { Line, Scatter, Bars, Stairs, Stems, Histogram };

const char* SeriesKindName(SeriesKind k);          // "line", "scatter", ...
bool SeriesKindFromName(const std::string& name, SeriesKind& out);
inline constexpr int kSeriesKindCount = 6;

struct Series {
    SeriesKind kind = SeriesKind::Line;
    std::string label;
    std::vector<double> x;    // ignored by Histogram
    std::vector<double> y;    // values (Histogram bins these)
    bool markers = false;     // Line only: draw circle markers on the points
    int bins = 0;             // Histogram only: 0 = automatic (Sturges)
    size_t Count() const { return kind == SeriesKind::Histogram ? y.size() : std::min(x.size(), y.size()); }
};

struct PlotSpec {
    std::string title;
    std::string xlabel;
    std::string ylabel;
    std::vector<Series> series;
};

// A published plot. `version` is bumped every time the spec is replaced so
// the panel can re-fit the axes only when the data actually changed.
struct NamedPlot {
    std::string name;
    PlotSpec spec;
    uint64_t version = 0;
    uint64_t fittedVersion = ~0ull;   // panel-side: last version the axes were fit to
};

}  // namespace plot
