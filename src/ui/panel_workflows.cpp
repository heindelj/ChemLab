// Workflows panel (a tab of the classic scene's right column) and the
// "Workflow: <name>" graph windows. Built-in workflows and the user's own
// (workflows/<name>.json) are listed in two collapsible sections; a row shows
// the trigger, the last outcome and a Run button, and double-clicking a row
// opens the workflow's graph in a window of its own, editable like any other
// graph. All logic lives in graph/workflow.h; this file is only the widgets.

#include <string>

#include <fmt/format.h>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/app_state.h"
#include "graph/graph_system.h"
#include "ui/ui.h"

namespace {

ImVec4 OutcomeColor(const graph::Workflow& w) {
    if (!w.lastRunOk) return {1.0f, 0.45f, 0.4f, 1.0f};
    if (w.lastRunSkipped) return {0.7f, 0.7f, 0.7f, 1.0f};
    return {0.55f, 0.9f, 0.55f, 1.0f};
}

bool TriggerCombo(graph::Workflow& w, float width) {
    bool changed = false;
    ImGui::PushItemWidth(width);
    if (ImGui::BeginCombo("##trigger", graph::TriggerName(w.trigger))) {
        for (graph::Trigger t : {graph::Trigger::Manual, graph::Trigger::FrameChange}) {
            if (ImGui::Selectable(graph::TriggerName(t), w.trigger == t) && w.trigger != t) {
                w.trigger = t;
                changed = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", graph::TriggerDescription(t));
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    return changed;
}

// One row of the list. Returns true when the row was double-clicked.
void WorkflowRow(AppState& state, graph::GraphSystem& gs, graph::Workflow& w) {
    ImGui::PushID(&w);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    // The name spans the row as a selectable so a double-click anywhere on it opens the graph.
    if (ImGui::Selectable(w.name.c_str(), w.graphOpen, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick |
                                                           ImGuiSelectableFlags_AllowOverlap)) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) w.graphOpen = true;
    }
    if (ImGui::IsItemHovered() && !w.description.empty() && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(360.0f);
        ImGui::TextUnformatted(w.description.c_str());
        ImGui::TextDisabled("double-click to open the graph");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::TableNextColumn();
    TriggerCombo(w, 80.0f);
    ImGui::TableNextColumn();
    if (ImGui::Checkbox("##enabled", &w.enabled)) {}
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enabled: the trigger may run it");
    ImGui::TableNextColumn();
    if (ImGui::SmallButton("Run")) gs.RunWorkflow(state, w);
    ImGui::TableNextColumn();
    if (w.runCount > 0) {
        ImGui::TextColored(OutcomeColor(w), "%s", w.lastRunSummary.c_str());
        if (ImGui::IsItemHovered() && w.lastRunSummary.size() > 40) ImGui::SetTooltip("%s", w.lastRunSummary.c_str());
    } else {
        ImGui::TextDisabled("not run yet");
    }
    ImGui::PopID();
}

void WorkflowTable(AppState& state, graph::GraphSystem& gs, const char* id, bool builtin) {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable(id, 5, flags)) return;
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ImGui::TableSetupColumn("trigger", ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("on", ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn("run", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("last run", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableHeadersRow();
    int shown = 0;
    for (graph::Workflow& w : gs.workflows) {
        if (w.builtin != builtin) continue;
        WorkflowRow(state, gs, w);
        ++shown;
    }
    if (shown == 0) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled(builtin ? "none" : "none yet: `workflow new <name>`, or Save a built-in under a new name");
    }
    ImGui::EndTable();
}

}  // namespace

void DrawWorkflowsPanel(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();
    static std::string newName;

    ImGui::TextDisabled("Programs expressed as node graphs, run by the executor. Double-click one to open its graph.");
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Built-in workflows", ImGuiTreeNodeFlags_DefaultOpen))
        WorkflowTable(state, gs, "##builtin_workflows", true);
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("User workflows", ImGuiTreeNodeFlags_DefaultOpen)) {
        WorkflowTable(state, gs, "##user_workflows", false);
        ImGui::PushItemWidth(160.0f);
        const bool entered = ImGui::InputTextWithHint("##new_workflow", "new workflow name", &newName, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if ((ImGui::Button("New") || entered) && !newName.empty()) {
            RunCommandLine(state, "workflow new " + newName);
            newName.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) gs.LoadWorkflows(state);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-read %s/", graph::WorkflowsDir().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s/", graph::WorkflowsDir().c_str());
    }
}

// One "Workflow: <name>" window per workflow whose graph is open.
void DrawWorkflowGraphWindows(AppState& state) {
    graph::GraphSystem& gs = state.GraphSys();
    int cascade = 0;
    for (size_t i = 0; i < gs.workflows.size(); ++i) {
        graph::Workflow& w = gs.workflows[i];
        if (!w.graphOpen) continue;
        const std::string title = fmt::format("Workflow: {}###workflow_graph_{}", w.name, i);
        const ImVec2 origin = ImGui::GetMainViewport()->WorkPos;
        ImGui::SetNextWindowPos(ImVec2(origin.x + 120.0f + 40.0f * (float)cascade, origin.y + 90.0f + 40.0f * (float)cascade),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(900, 520), ImGuiCond_FirstUseEver);
        ++cascade;
        if (ImGui::Begin(title.c_str(), &w.graphOpen, ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::Button("Run")) gs.RunWorkflow(state, w);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Compile (if the graph changed) and execute now.");
            ImGui::SameLine();
            ImGui::TextDisabled("trigger");
            ImGui::SameLine();
            TriggerCombo(w, 90.0f);
            ImGui::SameLine();
            ImGui::Checkbox("enabled", &w.enabled);
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextDisabled("name");
            ImGui::SameLine();
            ImGui::PushItemWidth(150.0f);
            ImGui::BeginDisabled(w.builtin);
            ImGui::InputText("##wf_name", &w.name);
            ImGui::EndDisabled();
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(w.builtin ? "Save as user copy" : "Save")) {
                std::string err;
                if (w.builtin) {
                    // Built-ins are immutable: saving makes an editable user copy next to it.
                    graph::Workflow copy = w;
                    copy.name = w.name + "-copy";
                    copy.builtin = false;
                    copy.graphOpen = true;
                    copy.runCount = 0;
                    copy.lastIoMessage = graph::SaveWorkflow(copy, err) ? "saved to " + graph::WorkflowPath(copy.name) : err;
                    w.graphOpen = false;
                    gs.workflows.push_back(std::move(copy));
                    ImGui::End();
                    return;   // `w` may have moved: draw the rest next frame
                }
                w.lastIoMessage = graph::SaveWorkflow(w, err) ? "saved to " + graph::WorkflowPath(w.name) : err;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Write %s", graph::WorkflowPath(w.name).c_str());
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextDisabled("%zu nodes%s | right-click the canvas to add", w.graph.nodes.size(), w.builtin ? " (built-in)" : "");
            if (!w.lastIoMessage.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", w.lastIoMessage.c_str());
            }
            if (w.runCount > 0) {
                ImGui::TextColored(OutcomeColor(w), "%s", w.lastRunSummary.c_str());
                if (w.lastRunOk && ImGui::IsItemHovered() && w.graph.program.Compiled() && ImGui::BeginTooltip()) {
                    ImGui::TextDisabled("per node (last run)");
                    for (const graph::Step& s : w.graph.program.steps)
                        ImGui::Text("%-16s %8.3f ms%s", s.node->title.c_str(), s.lastSeconds * 1e3, s.node->skipped ? "  skipped" : "");
                    ImGui::EndTooltip();
                }
            }
            ImGui::Separator();
            DrawGraphEditor(state, w.graph, fmt::format("workflow_graph_{}", i), nullptr);
        }
        ImGui::End();
    }
}
