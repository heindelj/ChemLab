#pragma once
// JSON (de)serialization of a graph::Graph: node types, titles, parameters,
// positions, per-instance pins (script nodes) and links. This is what
// `graph save` writes (graphs/<name>.json), so a sketched graph survives a restart.
// Node ids are preserved so saved files can be edited by hand or by an agent.

#include <string>

#include <nlohmann/json_fwd.hpp>

namespace graph {

struct Graph;

nlohmann::json GraphToJson(const Graph& g);
// Replaces the contents of `g`. Unknown node types are kept (with their saved
// pins) so a file from a newer build still loads; they report an error when
// evaluated. False + err on a malformed document (g is then left cleared).
bool GraphFromJson(const nlohmann::json& j, Graph& g, std::string& err);

bool SaveGraph(const Graph& g, const std::string& path, std::string& err);
bool LoadGraph(const std::string& path, Graph& g, std::string& err);

}  // namespace graph
