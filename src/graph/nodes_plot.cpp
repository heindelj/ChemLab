// Data + plotting nodes: load tabular data (CSV and friends), pull columns
// out of it, turn columns into plot series and publish named plots that the
// 2D Plot panel can switch between. Evaluation stays UI-free; the plot panel
// only reads AppState::plots.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"
#if !defined(__EMSCRIPTEN__)
#include "portable-file-dialogs.h"
#endif

#include "app/app_state.h"
#include "graph/graph.h"
#include "graph/node_registry.h"
#include "plot/plot_spec.h"

namespace graph {

namespace {

std::string TextParam(const Node& n, const std::string& key, const std::string& fallback = "") {
    auto it = n.params.find(key);
    if (it == n.params.end()) return fallback;
    const std::string* s = it->second.AsText();
    return s ? *s : fallback;
}

int64_t IntParam(const Node& n, const std::string& key, int64_t fallback) {
    auto it = n.params.find(key);
    int64_t v = fallback;
    if (it == n.params.end() || !it->second.AsInt(v)) return fallback;
    return v;
}

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// ---- CSV / TSV / whitespace-delimited parsing ----------------------------

char DetectDelimiter(const std::string& line) {
    if (line.find(',') != std::string::npos) return ',';
    if (line.find('\t') != std::string::npos) return '\t';
    if (line.find(';') != std::string::npos) return ';';
    return ' ';   // any run of whitespace
}

std::vector<std::string> SplitFields(const std::string& line, char delim) {
    std::vector<std::string> out;
    if (delim == ' ') {
        std::istringstream ss(line);
        std::string tok;
        while (ss >> tok) out.push_back(tok);
        return out;
    }
    std::string cur;
    bool quoted = false;
    for (char c : line) {
        if (c == '"') { quoted = !quoted; continue; }
        if (c == delim && !quoted) { out.push_back(Trim(cur)); cur.clear(); continue; }
        cur += c;
    }
    out.push_back(Trim(cur));
    return out;
}

bool ParseNumber(const std::string& s, double& out) {
    if (s.empty()) { out = std::nan(""); return true; }   // blank cell = missing
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) {
        std::string l = s;
        for (char& c : l) c = (char)std::tolower((unsigned char)c);
        if (l == "nan" || l == "na" || l == "null" || l == "none") { out = std::nan(""); return true; }
        return false;
    }
    while (*end && std::isspace((unsigned char)*end)) ++end;
    return *end == 0;
}

// Reads a delimited text table. The first non-comment line is the header
// when any of its fields is not a number; otherwise columns are named
// col1, col2, ... Lines starting with '#' are skipped. Non-numeric cells in
// data rows become NaN, so a text column simply plots as nothing.
std::string LoadTable(const std::string& path, Table& out) {
    std::ifstream in(path);
    if (!in) return fmt::format("cannot open '{}'", path);
    std::string line;
    bool haveHeader = false;
    char delim = ',';
    size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string t = Trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (!haveHeader) {
            delim = DetectDelimiter(t);
            std::vector<std::string> f = SplitFields(t, delim);
            bool allNumeric = true;
            double dummy;
            for (const std::string& s : f)
                if (!ParseNumber(s, dummy) || s.empty()) { allNumeric = false; break; }
            if (allNumeric) {
                for (size_t c = 0; c < f.size(); ++c) out.columns.push_back(fmt::format("col{}", c + 1));
            } else {
                out.columns = f;
            }
            out.data.assign(out.columns.size(), {});
            haveHeader = true;
            if (!allNumeric) continue;
        }
        std::vector<std::string> f = SplitFields(t, delim);
        if (f.size() < out.columns.size() && delim != ' ')
            f.resize(out.columns.size());   // short row: missing trailing cells
        if (f.size() != out.columns.size())
            return fmt::format("line {}: expected {} fields, got {}", lineNo, out.columns.size(), f.size());
        for (size_t c = 0; c < f.size(); ++c) {
            double v;
            if (!ParseNumber(f[c], v)) v = std::nan("");
            out.data[c].push_back(v);
        }
    }
    if (!haveHeader) return "file has no data";
    return "";
}

// ---- Load Table: a delimited text file as a Table ------------------------

std::string EvalLoadTable(AppState&, Node& n, const std::vector<const Value*>&, std::vector<Value>& out) {
    const std::string path = TextParam(n, "path");
    if (path.empty()) return "no file chosen";
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto stamp = fs::last_write_time(path, ec);
    if (ec) return fmt::format("cannot read '{}'", path);
    const int64_t mtime = (int64_t)stamp.time_since_epoch().count();
    // Cache the parsed table on the node: auto-run re-evaluates every tick and
    // re-parsing a big file each time would be silly.
    auto cached = n.params.find("_table");
    const bool fresh = cached != n.params.end() && cached->second.AsTable() && TextParam(n, "_path") == path &&
                       IntParam(n, "_mtime", -1) == mtime;
    if (!fresh) {
        Table t;
        if (std::string err = LoadTable(path, t); !err.empty()) return err;
        n.params["_table"].v = std::move(t);
        n.params["_path"] = Value::S(path);
        n.params["_mtime"] = Value::I(mtime);
    }
    const Table& t = *n.params["_table"].AsTable();
    out[0] = n.params["_table"];
    out[1] = Value::I((int64_t)t.Rows());
    return "";
}

bool BodyLoadTable(AppState&, Node& n) {
    bool changed = false;
    std::string path = TextParam(n, "path");
    namespace fs = std::filesystem;
    ImGui::Text("%s", path.empty() ? "<no file>" : fs::path(path).filename().string().c_str());
#if !defined(__EMSCRIPTEN__)
    if (ImGui::SmallButton("Browse...")) {
        auto sel = pfd::open_file("Choose a data file", ".",
                                  {"Tabular data", "*.csv *.tsv *.txt *.dat", "All files", "*"})
                       .result();
        if (!sel.empty()) {
            n.params["path"] = Value::S(sel.front());
            changed = true;
        }
    }
    ImGui::SameLine();
#endif
    if (ImGui::SmallButton("Reload")) {
        n.params.erase("_mtime");
        changed = true;
    }
    ImGui::PushItemWidth(200.0f);
    if (ImGui::InputText("path", &path)) { n.params["path"] = Value::S(path); changed = true; }
    ImGui::PopItemWidth();
    if (auto it = n.params.find("_table"); it != n.params.end())
        if (const Table* t = it->second.AsTable())
            ImGui::TextDisabled("%zu rows x %zu columns", t->Rows(), t->Cols());
    return changed;
}

// ---- Column: one column of a table as a FloatVec --------------------------

std::string EvalColumn(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    if (!in[0]) return "input 'table' not connected";
    const Table* t = in[0]->AsTable();
    if (!t) return "wrong input type on 'table'";
    n.params["_columns"].v = Labels(t->columns);   // for the picker in the body
    const std::string want = TextParam(n, "column");
    int c = -1;
    if (want.empty()) c = t->Cols() ? 0 : -1;
    else {
        c = t->FindColumn(want);
        if (c < 0) {   // allow a 1-based index too
            char* end = nullptr;
            const long k = std::strtol(want.c_str(), &end, 10);
            if (end && *end == 0 && k >= 1 && k <= (long)t->Cols()) c = (int)k - 1;
        }
    }
    if (c < 0) return fmt::format("no column '{}'", want);
    out[0].v = t->data[(size_t)c];
    out[1] = Value::S(t->columns[(size_t)c]);
    return "";
}

bool BodyColumn(AppState&, Node& n) {
    bool changed = false;
    std::string col = TextParam(n, "column");
    ImGui::PushItemWidth(140.0f);
    if (ImGui::InputText("column", &col)) { n.params["column"] = Value::S(col); changed = true; }
    ImGui::PopItemWidth();
    auto it = n.params.find("_columns");
    const Labels* names = it == n.params.end() ? nullptr : it->second.AsLabels();
    if (!names || names->empty()) {
        ImGui::TextDisabled("(run the graph to list columns)");
        return changed;
    }
    // Plain radio buttons: combos/popups are off-limits inside the editor canvas.
    for (size_t i = 0; i < names->size() && i < 12; ++i) {
        const bool active = (*names)[i] == col || (col.empty() && i == 0);
        if (ImGui::RadioButton((*names)[i].c_str(), active) && !active) {
            n.params["column"] = Value::S((*names)[i]);
            changed = true;
        }
    }
    if (names->size() > 12) ImGui::TextDisabled("(+%zu more: type the name)", names->size() - 12);
    return changed;
}

// ---- Series: x/y values + a kind -> one plot series -----------------------

std::string EvalSeries(AppState&, Node& n, const std::vector<const Value*>& in, std::vector<Value>& out) {
    if (!in[1]) return "input 'y' not connected";
    const std::vector<double>* y = in[1]->AsFloatVec();
    if (!y) return "wrong input type on 'y'";
    Series s;
    plot::SeriesKindFromName(TextParam(n, "kind", "line"), s.kind);
    s.markers = IntParam(n, "markers", 0) != 0;
    s.bins = (int)IntParam(n, "bins", 0);
    s.y = *y;
    if (in[0]) {
        const std::vector<double>* x = in[0]->AsFloatVec();
        if (!x) return "wrong input type on 'x'";
        if (x->size() != y->size()) return fmt::format("x has {} values, y has {}", x->size(), y->size());
        s.x = *x;
    } else {
        s.x.resize(y->size());
        for (size_t i = 0; i < s.x.size(); ++i) s.x[i] = (double)i;
    }
    // Label: explicit param, else the 'label' pin (a Column node's name), else the node title.
    s.label = TextParam(n, "label");
    if (s.label.empty() && in[2]) if (const std::string* l = in[2]->AsText()) s.label = *l;
    if (s.label.empty()) s.label = n.title;
    out[0].v = std::move(s);
    return "";
}

bool BodySeries(AppState&, Node& n) {
    bool changed = false;
    plot::SeriesKind kind = plot::SeriesKind::Line;
    plot::SeriesKindFromName(TextParam(n, "kind", "line"), kind);
    for (int i = 0; i < plot::kSeriesKindCount; ++i) {
        const auto k = (plot::SeriesKind)i;
        if (i % 3) ImGui::SameLine();
        if (ImGui::RadioButton(plot::SeriesKindName(k), kind == k) && kind != k) {
            n.params["kind"] = Value::S(plot::SeriesKindName(k));
            kind = k;
            changed = true;
        }
    }
    if (kind == plot::SeriesKind::Line) {
        bool markers = IntParam(n, "markers", 0) != 0;
        if (ImGui::Checkbox("markers", &markers)) { n.params["markers"] = Value::I(markers ? 1 : 0); changed = true; }
    }
    if (kind == plot::SeriesKind::Histogram) {
        int bins = (int)IntParam(n, "bins", 0);
        ImGui::PushItemWidth(90.0f);
        if (ImGui::InputInt("bins (0=auto)", &bins)) { n.params["bins"] = Value::I(std::max(bins, 0)); changed = true; }
        ImGui::PopItemWidth();
    }
    std::string label = TextParam(n, "label");
    ImGui::PushItemWidth(140.0f);
    if (ImGui::InputText("label", &label)) { n.params["label"] = Value::S(label); changed = true; }
    ImGui::PopItemWidth();
    return changed;
}

// ---- Plot 2D: series -> a named plot the 2D Plot panel can show -----------

constexpr int kPlotSeriesPins = 4;

std::string EvalPlot2D(AppState& s, Node& n, const std::vector<const Value*>& in, std::vector<Value>&) {
    plot::PlotSpec spec;
    spec.title = TextParam(n, "title");
    spec.xlabel = TextParam(n, "xlabel");
    spec.ylabel = TextParam(n, "ylabel");
    for (int k = 0; k < kPlotSeriesPins; ++k) {
        if (!in[k]) continue;
        const Series* sr = in[k]->AsSeries();
        if (!sr) return fmt::format("wrong input type on 's{}'", k + 1);
        spec.series.push_back(*sr);
    }
    if (spec.series.empty()) return "no series connected";
    std::string name = TextParam(n, "name");
    if (name.empty()) name = n.title;
    // Renamed since the last run: retire the old entry so the picker stays tidy.
    const std::string previous = TextParam(n, "_published");
    if (!previous.empty() && previous != name) s.RemovePlot(previous);
    s.PublishPlot(name, std::move(spec));
    n.params["_published"] = Value::S(name);
    return "";
}

bool BodyPlot2D(AppState& s, Node& n) {
    bool changed = false;
    ImGui::PushItemWidth(160.0f);
    for (const char* key : {"name", "title", "xlabel", "ylabel"}) {
        std::string v = TextParam(n, key);
        if (ImGui::InputText(key, &v)) { n.params[key] = Value::S(v); changed = true; }
    }
    ImGui::PopItemWidth();
    const std::string published = TextParam(n, "_published");
    if (!published.empty() && s.FindPlot(published)) {
        const bool shown = s.SelectedPlotName() == published;
        if (shown) ImGui::TextDisabled("shown in 2D Plot");
        else if (ImGui::SmallButton("Show")) {
            s.SelectPlot(published);
            s.PanelOpen("plot_2d") = true;
        }
    } else {
        ImGui::TextDisabled("(run the graph to publish)");
    }
    return changed;
}

}  // namespace

void RegisterPlotNodes(NodeTypeRegistry& r) {
    r.Register({"data.load_table", "Load Table", "Data",
                "Reads a CSV/TSV/whitespace-delimited file into a table (first row = header).",
                {},
                {{"table", ValueType::Table}, {"rows", ValueType::Int}},
                &EvalLoadTable, &BodyLoadTable});
    r.Register({"data.column", "Column", "Data",
                "One column of a table, by name or 1-based index.",
                {{"table", ValueType::Table}},
                {{"values", ValueType::FloatVec}, {"name", ValueType::Text}},
                &EvalColumn, &BodyColumn});
    r.Register({"plot.series", "Series", "Plot",
                "Turns x/y values into a line, scatter, bar, stairs, stem or histogram series.",
                {{"x", ValueType::FloatVec}, {"y", ValueType::FloatVec}, {"label", ValueType::Text}},
                {{"series", ValueType::Series}},
                &EvalSeries, &BodySeries});
    r.Register({"plot.plot2d", "Plot 2D", "Plot",
                "Publishes its series as a named plot selectable in the 2D Plot panel.",
                {{"s1", ValueType::Series}, {"s2", ValueType::Series}, {"s3", ValueType::Series}, {"s4", ValueType::Series}},
                {},
                &EvalPlot2D, &BodyPlot2D});
}

}  // namespace graph
