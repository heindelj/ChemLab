#pragma once
// The execution system behind workflows.
//
// A graph is a *description* of a computation; nothing about it runs until it
// is compiled into a Program and handed to Execute(). Compilation resolves,
// once, everything that would otherwise be looked up per run: the dependency
// order, which output slot feeds which input pin, and -- for native nodes --
// the kernel function pointer behind the node. Running a Program is then a
// flat loop of function calls over pre-sized value slots, with no map lookups
// and no allocation beyond what the kernels themselves need.
//
// Two flavours of node take part:
//
//   * Native kernels. A Kernel is a plain function pointer taking typed Values
//     in and writing Values out. It sees the node's parameters but not the
//     application, so it is pure, benchmarkable and callable from anywhere
//     (a command, a test, another kernel). Kernels live in the KernelTable
//     under a stable id ("chem.neighbor_list"); a NodeTypeSpec names the
//     kernel it wraps in NodeTypeSpec::kernel. This is the table the user
//     asked for: look the id up, get the compiled function.
//
//   * App-bound nodes (sources such as Chemical Data, sinks such as Apply
//     Topology, anything with an EvalFn only). The compiler wraps their
//     EvalFn so they run in the same loop; they are what connects a program
//     to AppState at either end.
//
// Non-native work (a python script) is just another app-bound node today: the
// script node's EvalFn spawns the interpreter. A future ScriptKernel would sit
// in the same table with an extra description of how to marshal its inputs.
//
// Control flow: a kernel may *close* its outputs (KernelArgs::skip). Every
// node downstream of a closed output is skipped rather than run -- that is how
// a Gate node fences off the rest of a workflow when its condition is false.

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "graph/value.h"

struct AppState;

namespace graph {

struct Graph;
struct Node;
struct NodeTypeSpec;

// What a kernel sees: its inputs (null when unconnected), its outputs
// (pre-sized to the node's output pins), the node's parameters (widget
// state), and a flag it may set to close its outputs.
struct KernelArgs {
    const Value* const* in = nullptr;
    size_t nin = 0;
    Value* out = nullptr;
    size_t nout = 0;
    const std::map<std::string, Value>* params = nullptr;
    bool skip = false;   // set by the kernel: outputs are closed, skip downstream

    // Parameter helpers (fallback when absent or of another type).
    double FloatParam(const char* key, double fallback) const;
    int64_t IntParam(const char* key, int64_t fallback) const;
    std::string TextParam(const char* key, const std::string& fallback) const;
};

// Returns "" on success or an error message (shown on the node).
using Kernel = std::string (*)(KernelArgs&);

struct KernelInfo {
    std::string id;            // "array.gather"
    std::string description;   // one line
    Kernel fn = nullptr;
};

class KernelTable {
public:
    void Register(const std::string& id, Kernel fn, const std::string& description = "");
    Kernel Find(const std::string& id) const;   // null when unknown
    const std::vector<KernelInfo>& All() const { return kernels; }

private:
    std::vector<KernelInfo> kernels;
};

// The process-wide table, native kernels registered on first use.
KernelTable& Kernels();
void RegisterArrayKernels(KernelTable& t);   // kernels_array.cpp
void RegisterChemKernels(KernelTable& t);    // kernels_chem.cpp

// ---- compiled programs ----

struct Step {
    Node* node = nullptr;
    const NodeTypeSpec* spec = nullptr;
    Kernel kernel = nullptr;                 // native: call this; null: spec->evaluate
    std::vector<std::pair<Node*, int>> in;   // per input pin: (upstream node, output pin), or (null, -1)
    double lastSeconds = 0.0;                // wall time of the last run of this step
};

struct Program {
    std::vector<Step> steps;      // dependency order
    uint64_t graphVersion = ~0ull;   // Graph::version this was compiled from
    const Graph* owner = nullptr;    // the Graph object it was compiled from (steps point into its nodes;
                                     // a copied/moved Graph therefore recompiles on first use)
    std::string compileError;     // "" when the program is runnable
    bool Compiled() const { return compileError.empty() && graphVersion != ~0ull; }
};

struct ExecStats {
    int ran = 0, skipped = 0, failed = 0;
    double seconds = 0.0;         // whole program
    std::string firstError;       // "<node>: <error>" or ""
};

// Build `out` from `g`. "" or the error (a cycle, an unknown node type).
std::string Compile(Graph& g, Program& out);
// Compile only when the graph changed since the last compile.
std::string EnsureCompiled(Graph& g, Program& p);
// Run every step in order, writing results into each node's outValues
// (and Node::error / Node::skipped). `state` is only handed to app-bound nodes.
ExecStats Execute(AppState& state, Program& p);

}  // namespace graph
