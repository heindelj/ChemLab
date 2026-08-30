#pragma once
// A registry of named commands. Every user-facing action in ChemLab is
// reachable through here, so the command bar, menus/buttons and (later) AI
// agents all drive the application through the same, inspectable surface.

#include <functional>
#include <map>
#include <string>
#include <vector>

struct AppState;

struct CommandResult {
    bool ok = true;
    std::string message;   // human readable; may be multi-line

    static CommandResult Ok(std::string msg = "") { return {true, std::move(msg)}; }
    static CommandResult Error(std::string msg) { return {false, std::move(msg)}; }
};

struct CommandArgs {
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags;   // --key value / --key (value "true")

    size_t size() const { return positional.size(); }
    const std::string& operator[](size_t i) const { return positional[i]; }
    bool Has(const std::string& flag) const { return flags.count(flag) != 0; }
    std::string Flag(const std::string& flag, const std::string& fallback = "") const {
        auto it = flags.find(flag);
        return it == flags.end() ? fallback : it->second;
    }
};

using CommandHandler = std::function<CommandResult(AppState&, const CommandArgs&)>;

struct CommandSpec {
    std::string name;
    std::string usage;        // e.g. "frame <n|next|prev|first|last>"
    std::string description;  // one line
    std::string category;     // "file", "view", "selection", ...
    CommandHandler handler;
};

class CommandRegistry {
public:
    void Register(CommandSpec spec);
    bool Has(const std::string& name) const { return commands.count(name) != 0; }
    const CommandSpec* Find(const std::string& name) const;

    // Parse and run one line, e.g. `color 255 0 0 --alpha 0.5`.
    CommandResult Execute(AppState& state, const std::string& line);
    // Run several lines separated by ';' or newlines. Stops at first error.
    CommandResult ExecuteScript(AppState& state, const std::string& script);

    std::vector<std::string> Complete(const std::string& prefix) const;
    std::vector<const CommandSpec*> All() const;
    std::string HelpText(const std::string& name = "") const;

    static std::vector<std::string> Tokenize(const std::string& line);
    static CommandArgs ParseArgs(const std::vector<std::string>& tokens);

private:
    std::map<std::string, CommandSpec> commands;
};

// Registers every built-in command (see commands.cpp for the list).
void RegisterBuiltinCommands(CommandRegistry& registry);
