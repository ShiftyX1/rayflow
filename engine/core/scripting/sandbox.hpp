#pragma once

#include "lua_state.hpp"

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace engine::scripting {

// Sandbox configuration for map scripts
struct SandboxConfig {
    // Memory limits
    std::size_t maxMemoryMB{64};
    
    // Execution limits
    std::size_t maxInstructionsPerCall{10000000};  // 10M
    double maxExecutionTimeSec{5.0};
    
    // Allowed modules (empty = none)
    std::unordered_set<std::string> allowedModules;
    
    // Custom print handler
    std::function<void(const std::string&)> printHandler;
    
    // Error handler (called on script errors)
    std::function<void(const std::string&)> errorHandler;
    
    static SandboxConfig default_for_maps() {
        SandboxConfig cfg;
        cfg.maxMemoryMB = 32;  // 32 MB for map scripts
        cfg.maxInstructionsPerCall = 5000000;  // 5M per call
        cfg.maxExecutionTimeSec = 2.0;  // 2 seconds max
        return cfg;
    }
    
    static SandboxConfig default_for_ui() {
        SandboxConfig cfg;
        cfg.maxMemoryMB = 16;  // 16 MB for UI scripts
        cfg.maxInstructionsPerCall = 1000000;  // 1M per call
        cfg.maxExecutionTimeSec = 0.5;  // 500ms max (UI must be responsive)
        return cfg;
    }
    
    ScriptLimits to_script_limits() const {
        ScriptLimits limits;
        limits.maxMemoryBytes = maxMemoryMB * 1024 * 1024;
        limits.maxInstructions = maxInstructionsPerCall;
        limits.maxExecutionTimeSec = maxExecutionTimeSec;
        return limits;
    }
};

// Validation result for scripts
struct ValidationResult {
    bool valid{false};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    explicit operator bool() const { return valid; }
};

// Sandbox utility functions
class Sandbox {
public:
    // Validate a script without executing it
    static ValidationResult validate_script(const std::string& script);
    
    // Check if a script uses any forbidden functions
    static ValidationResult check_forbidden_calls(const std::string& script);
    
    // Create a sandboxed environment with the given config
    static std::unique_ptr<LuaState> create(const SandboxConfig& config);
    
    // List of functions that are forbidden in sandboxed scripts
    static const std::vector<std::string>& forbidden_functions();
    
    // List of functions that are allowed in sandboxed scripts
    static const std::vector<std::string>& safe_functions();
};

// Script validation helper
class ScriptValidator {
public:
    ScriptValidator();
    
    // Add a pattern to check for (e.g., "os%.", "io%.")
    void add_forbidden_pattern(const std::string& pattern);
    
    // Add a warning pattern (not forbidden but discouraged)
    void add_warning_pattern(const std::string& pattern, const std::string& message);
    
    // Validate a script
    ValidationResult validate(const std::string& script) const;

private:
    std::vector<std::string> forbiddenPatterns_;
    std::vector<std::pair<std::string, std::string>> warningPatterns_;
};

} // namespace engine::scripting
