#include "graph/py_runner.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <fmt/format.h>

#if defined(_WIN32)
#include <process.h>
#define POPEN _popen
#define PCLOSE _pclose
#define GETPID _getpid
#else
#include <sys/wait.h>
#include <unistd.h>
#define POPEN popen
#define PCLOSE pclose
#define GETPID getpid
#endif

namespace graph {

#if defined(__EMSCRIPTEN__)

bool ScriptingAvailable() { return false; }

RunResult RunScript(const std::string&, const std::string&, const std::string&, const std::string&) {
    RunResult r;
    r.error = "script nodes are not available in the web build";
    return r;
}

#else

bool ScriptingAvailable() { return true; }

namespace {

std::string ReadAll(const std::string& path, size_t maxBytes = 4000) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (s.size() > maxBytes) s = "..." + s.substr(s.size() - maxBytes);
    return s;
}

}  // namespace

RunResult RunScript(const std::string& python, const std::string& script, const std::string& args,
                    const std::string& stdinData, const ScriptEnv& env) {
    RunResult r;
    if (script.empty()) {
        r.error = "no script set";
        return r;
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path tmp = fs::temp_directory_path(ec);
    if (ec) {
        r.error = "no temp directory available";
        return r;
    }
    static int counter = 0;
    const std::string tag = fmt::format("chemlab_node_{}_{}", (int)GETPID(), counter++);
    const fs::path inFile = tmp / (tag + "_in.json");
    const fs::path errFile = tmp / (tag + "_err.txt");
    {
        std::ofstream f(inFile, std::ios::binary);
        f << stdinData;
        if (!f) {
            r.error = "failed to write the request temp file";
            return r;
        }
    }
    // Quoting is POSIX-shell style; fine on macOS/Linux. (Windows would want
    // CreateProcess instead of cmd.exe quoting -- revisit if/when it matters.)
    std::string envPrefix;   // VAR='value' ... before the command (single quotes: no expansion)
    for (const auto& [k, v] : env) {
        std::string quoted = "'";
        for (char ch : v) quoted += ch == '\'' ? std::string("'\\''") : std::string(1, ch);
        quoted += "'";
        envPrefix += fmt::format("{}={} ", k, quoted);
    }
    const std::string cmd = fmt::format("{}\"{}\" \"{}\"{}{} < \"{}\" 2> \"{}\"", envPrefix, python, script,
                                        args.empty() ? "" : " ", args, inFile.string(), errFile.string());
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        r.error = fmt::format("failed to launch '{}'", python);
        fs::remove(inFile, ec);
        fs::remove(errFile, ec);
        return r;
    }
    char buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), pipe)) > 0) r.output.append(buf, got);
    const int status = PCLOSE(pipe);
#if defined(_WIN32)
    r.exitCode = status;
#else
    r.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    r.ok = r.exitCode == 0;
    if (!r.ok) {
        const std::string tail = ReadAll(errFile.string());
        r.error = fmt::format("script exited with status {}{}", r.exitCode,
                              tail.empty() ? "" : "\n" + tail);
    }
    fs::remove(inFile, ec);
    fs::remove(errFile, ec);
    return r;
}

#endif  // __EMSCRIPTEN__

}  // namespace graph
