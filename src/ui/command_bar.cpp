// The command bar pinned to the bottom of the window, plus the Console panel
// that shows the full log. Every line typed here goes through the
// CommandRegistry, which is the same surface an AI agent will use later.

#include <algorithm>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "app/actions.h"
#include "ui/ui.h"

namespace {

int gHistoryPos = -1;   // -1 = editing a fresh line

std::string FirstLine(const std::string& s) {
    const auto nl = s.find('\n');
    return nl == std::string::npos ? s : s.substr(0, nl) + " ...";
}

int CommandInputCallback(ImGuiInputTextCallbackData* data) {
    AppState& state = *static_cast<AppState*>(data->UserData);
    switch (data->EventFlag) {
        case ImGuiInputTextFlags_CallbackCompletion: {
            // Complete the first word only; arguments are the command's business.
            const std::string current(data->Buf, data->CursorPos);
            if (current.find(' ') != std::string::npos) break;
            const auto matches = state.commands.Complete(current);
            if (matches.empty()) break;
            if (matches.size() == 1) {
                data->DeleteChars(0, data->CursorPos);
                data->InsertChars(0, (matches[0] + " ").c_str());
            } else {
                // Insert the longest common prefix and list the options.
                std::string prefix = matches[0];
                for (const auto& m : matches) {
                    size_t i = 0;
                    while (i < prefix.size() && i < m.size() && prefix[i] == m[i]) i++;
                    prefix.resize(i);
                }
                data->DeleteChars(0, data->CursorPos);
                data->InsertChars(0, prefix.c_str());
                state.lastCommandResult = fmt::format("{}", fmt::join(matches, "  "));
                state.lastCommandOk = true;
            }
            break;
        }
        case ImGuiInputTextFlags_CallbackHistory: {
            const int prev = gHistoryPos;
            const int n = (int)state.commandHistory.size();
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (gHistoryPos == -1) gHistoryPos = n - 1;
                else if (gHistoryPos > 0) gHistoryPos--;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (gHistoryPos != -1 && ++gHistoryPos >= n) gHistoryPos = -1;
            }
            if (prev != gHistoryPos) {
                const std::string line = gHistoryPos >= 0 ? state.commandHistory[gHistoryPos] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, line.c_str());
            }
            break;
        }
        default: break;
    }
    return 0;
}

ImVec4 LevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Error: return ImVec4(1.0f, 0.42f, 0.42f, 1.0f);
        case LogLevel::Warning: return ImVec4(1.0f, 0.75f, 0.3f, 1.0f);
        case LogLevel::Command: return ImVec4(0.55f, 0.75f, 1.0f, 1.0f);
        case LogLevel::Result: return ImVec4(0.8f, 0.85f, 0.9f, 1.0f);
        default: return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
}

}  // namespace

void RunCommandLine(AppState& state, const std::string& line) {
    if (line.find_first_not_of(" \t") == std::string::npos) return;
    Log(state, LogLevel::Command, "> " + line);
    if (state.commandHistory.empty() || state.commandHistory.back() != line) state.commandHistory.push_back(line);
    gHistoryPos = -1;
    const CommandResult result = state.commands.ExecuteScript(state, line);
    state.lastCommandOk = result.ok;
    state.lastCommandResult = result.message;
    if (!result.message.empty()) Log(state, result.ok ? LogLevel::Result : LogLevel::Error, result.message);
    // Multi-line answers (help, lists) do not fit the bar: open the console.
    if (result.message.find('\n') != std::string::npos) state.PanelOpen("console") = true;
}

float CommandBarHeight() { return ImGui::GetFrameHeight() + 12.0f; }

void DrawCommandBar(AppState& state) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float height = CommandBarHeight();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, height));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##CommandBar", nullptr, flags);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), ">");
    ImGui::SameLine();

    // Result / status text takes the right 45% of the bar; a console toggle sits at the far right.
    const float consoleButtonWidth = 70.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float inputWidth = avail * 0.55f;
    const float statusWidth = avail - inputWidth - consoleButtonWidth - ImGui::GetStyle().ItemSpacing.x * 2;

    if (state.focusCommandBar) {
        ImGui::SetKeyboardFocusHere();
        state.focusCommandBar = false;
    }
    ImGui::SetNextItemWidth(inputWidth);
    const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
                                           ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EscapeClearsAll;
    if (ImGui::InputTextWithHint("##command", "type a command (help)  -  Ctrl+K to focus, Tab to complete, Up/Down for history",
                                 &state.commandInput, inputFlags, CommandInputCallback, &state)) {
        RunCommandLine(state, state.commandInput);
        state.commandInput.clear();
        ImGui::SetKeyboardFocusHere(-1);   // keep typing
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, state.lastCommandOk ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
                                                             : ImVec4(1.0f, 0.42f, 0.42f, 1.0f));
    const std::string status = FirstLine(state.lastCommandResult);
    ImGui::AlignTextToFramePadding();
    // Clip rather than wrap: the bar is one line tall.
    ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                        ImVec2(ImGui::GetCursorScreenPos().x + statusWidth, ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight()), true);
    ImGui::TextUnformatted(status.c_str());
    ImGui::PopClipRect();
    ImGui::PopStyleColor();
    if (!state.lastCommandResult.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", state.lastCommandResult.c_str());

    ImGui::SameLine(ImGui::GetWindowWidth() - consoleButtonWidth - 8.0f);
    if (ImGui::SmallButton(state.PanelOpen("console") ? "Console v" : "Console ^")) state.PanelOpen("console") = !state.PanelOpen("console");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void DrawConsolePanel(AppState& state) {
    static bool autoScroll = true;
    static std::string filter;
    if (ImGui::SmallButton("Clear")) state.log.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##filter", "filter", &filter);
    ImGui::Separator();

    ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const LogEntry& e : state.log) {
        if (!filter.empty() && e.text.find(filter) == std::string::npos) continue;
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(e.level));
        ImGui::TextUnformatted(e.text.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    static size_t lastCount = 0;
    if (autoScroll && state.log.size() != lastCount) ImGui::SetScrollHereY(1.0f);
    lastCount = state.log.size();
    ImGui::EndChild();
}
