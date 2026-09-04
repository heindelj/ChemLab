#pragma once
// Runs external scripts for graph nodes. Protocol v0 (JSON over stdio):
//
//   describe:  <python> <script> --describe
//              stdout: {"name": "...", "inputs":  [{"name": "positions", "type": "positions"}, ...],
//                                      "outputs": [{"name": "distance",  "type": "float"}, ...]}
//   run:       <python> <script>          (request on stdin)
//              stdin:  {"inputs": {"positions": [[x,y,z], ...], "i": 0, ...}}
//              stdout: {"outputs": {"distance": 1.23, ...}}   or   {"error": "message"}
//
// Types: float, int, text, floatvec, positions (N x [x,y,z]), labels, intvec,
// chemdata, any. chemdata is ChemicalData (see chemical_data.h) as JSON:
//   {"natoms", "R": [flat 3N], "Z": [N ints], "topologies": [{"name", "pairs": [[i,j], ...]}],
//    "cell": [9 numbers]|null, "fields": [{"name","dtype","offset","count"}], "bytes": "<base64>"}
// ChemLab only sees the JSON on stdout -- the script can do whatever it wants
// (numpy, ASE, a subprocess of its own, another language entirely).

#include <string>
#include <utility>
#include <vector>

namespace graph {

using ScriptEnv = std::vector<std::pair<std::string, std::string>>;   // extra environment variables

struct RunResult {
    bool ok = false;        // process spawned and exited with status 0
    int exitCode = -1;
    std::string output;     // captured stdout
    std::string error;      // spawn failure / non-zero exit, with a stderr tail
};

// Blocking; stdinData is piped to the script via a temp file.
RunResult RunScript(const std::string& python, const std::string& script, const std::string& args,
                    const std::string& stdinData, const ScriptEnv& env = {});

// False in the web build (no subprocesses there).
bool ScriptingAvailable();

}  // namespace graph
