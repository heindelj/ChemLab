#pragma once
// Every state change with a side effect on the scene. Widgets and commands
// both call these so that the UI and the command bar can never disagree.

#include <string>

#include "app/app_state.h"

// ---- logging ----
void Log(AppState& state, LogLevel level, const std::string& text);
inline void LogInfo(AppState& s, const std::string& t) { Log(s, LogLevel::Info, t); }
inline void LogError(AppState& s, const std::string& t) { Log(s, LogLevel::Error, t); }

// ---- projects ----
CommandResult NewProject(AppState& state, const std::string& directory, const std::string& name);
CommandResult OpenProject(AppState& state, const std::string& path);
CommandResult SaveProject(AppState& state);
CommandResult CloseProject(AppState& state);
// Copies the live session (structures, view settings, measurements) into
// state.project->config; called by SaveProject.
void CaptureProjectState(AppState& state);
// Path relative to the project root when a project is open (for display/save).
std::string ProjectRelative(const AppState& state, const std::string& absolutePath);

// ---- structures / frames ----
CommandResult LoadStructureFile(AppState& state, const std::string& path, bool makeActive = true);
CommandResult SetActiveStructure(AppState& state, int index);
CommandResult RemoveStructure(AppState& state, int index);
CommandResult RenameStructure(AppState& state, int index, const std::string& name);
CommandResult SetFrame(AppState& state, int frameIndex);
CommandResult StepFrame(AppState& state, int delta);
void UpdatePlayback(AppState& state);
void UpdateFileWatch(AppState& state);

// Rebuild the GPU model of the active frame (also called lazily when dirty).
void RebuildModel(AppState& state);
void MarkGeometryChanged(AppState& state);   // positions/bonds changed, keep colours

// ---- camera ----
void ResetCamera(AppState& state);
void LookDownAxis(AppState& state, int axis, bool flip);

// ---- selection ----
CommandResult SelectAtoms(AppState& state, const std::set<int>& atoms, bool add);
CommandResult SelectByElement(AppState& state, const std::string& element, bool add);
CommandResult SelectAll(AppState& state);
CommandResult SelectNone(AppState& state);
CommandResult InvertSelection(AppState& state);
CommandResult ToggleAtomSelected(AppState& state, int atom);

// ---- colour ----
CommandResult ColorSelection(AppState& state, Color color);
CommandResult SetSelectionAlpha(AppState& state, float alpha);
CommandResult ResetColors(AppState& state);

// ---- measurements ----
// Feed one clicked atom into the pending distance/angle/dihedral selection.
void MeasurementClick(AppState& state, int atom);
void CommitPendingMeasurement(AppState& state);
void CancelPendingMeasurement(AppState& state);
CommandResult AddMeasurement(AppState& state, const std::vector<int>& atoms);
CommandResult RemoveMeasurement(AppState& state, int index);
CommandResult ClearMeasurements(AppState& state);
// Value of a measurement for the given frame, or NaN.
double MeasurementValue(const ChemicalData& atoms, const Measurement& m);
std::string MeasurementLabel(const Measurement& m);

// ---- calculations ----
CommandResult RecomputeBonds(AppState& state);

// ---- export ----
CommandResult SaveScreenshot(AppState& state, const std::string& path, int width, int height, bool transparent);
CommandResult ExportXYZ(AppState& state, const std::string& path, bool allFrames);

// ---- utility ----
bool ParseColor(const std::vector<std::string>& tokens, size_t start, Color& out);
Color ImVec4ToColor(const float rgba[4]);
