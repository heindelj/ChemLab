#include "plot/plot_spec.h"

namespace plot {

const char* SeriesKindName(SeriesKind k) {
    switch (k) {
        case SeriesKind::Line: return "line";
        case SeriesKind::Scatter: return "scatter";
        case SeriesKind::Bars: return "bars";
        case SeriesKind::Stairs: return "stairs";
        case SeriesKind::Stems: return "stems";
        case SeriesKind::Histogram: return "histogram";
    }
    return "line";
}

bool SeriesKindFromName(const std::string& name, SeriesKind& out) {
    for (int i = 0; i < kSeriesKindCount; ++i) {
        const auto k = (SeriesKind)i;
        if (name == SeriesKindName(k)) { out = k; return true; }
    }
    return false;
}

}  // namespace plot
