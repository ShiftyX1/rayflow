#include "sandbox.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <regex>
#include <sstream>

namespace engine::scripting {

namespace {

const std::vector<std::string> kForbiddenFunctions = {
    "os",
    "io",
    "debug",
    "loadfile",
    "dofile",
    "load",
    "loadstring",
    "require",
    "package",
    "collectgarbage",
    "rawget",
    "rawset",
    "rawequal",
    "setmetatable",
    "getfenv",
    "setfenv",
    "newproxy",
    "gcinfo",
    "module",
};

const std::vector<std::string> kSafeFunctions = {
    // Base
    "print",
    "type",
    "tostring",
    "tonumber",
    "pairs",
    "ipairs",
    "next",
    "select",
    "unpack",
    "pcall",
    "xpcall",
    "error",
    "assert",
    "getmetatable",  // Read-only is ok
    
    // String
    "string.byte",
    "string.char",
    "string.find",
    "string.format",
    "string.gmatch",
    "string.gsub",
    "string.len",
    "string.lower",
    "string.match",
    "string.rep",
    "string.reverse",
    "string.sub",
    "string.upper",
    
    // Table
    "table.concat",
    "table.insert",
    "table.remove",
    "table.sort",
    "table.unpack",
    "table.pack",
    
    // Math
    "math.abs",
    "math.acos",
    "math.asin",
    "math.atan",
    "math.atan2",
    "math.ceil",
    "math.cos",
    "math.cosh",
    "math.deg",
    "math.exp",
    "math.floor",
    "math.fmod",
    "math.frexp",
    "math.huge",
    "math.ldexp",
    "math.log",
    "math.log10",
    "math.max",
    "math.min",
    "math.modf",
    "math.pi",
    "math.pow",
    "math.rad",
    "math.random",
    "math.randomseed",
    "math.sin",
    "math.sinh",
    "math.sqrt",
    "math.tan",
    "math.tanh",
    
    // Coroutine
    "coroutine.create",
    "coroutine.resume",
    "coroutine.running",
    "coroutine.status",
    "coroutine.wrap",
    "coroutine.yield",
};

} // namespace

ValidationResult Sandbox::validate_script(const std::string& script) {
    ValidationResult result;
    result.valid = true;
    
    // Create a temporary Lua state for syntax checking
    auto state = create_sandboxed_state();
    if (!state) {
        result.valid = false;
        result.errors.push_back("Failed to create Lua state");
        return result;
    }
    
    auto loadResult = state->load(script);
    if (!loadResult) {
        result.valid = false;
        result.errors.push_back(loadResult.error);
        return result;
    }
    
    // Check for forbidden function usage
    auto forbiddenCheck = check_forbidden_calls(script);
    if (!forbiddenCheck.valid) {
        result.valid = false;
        result.errors.insert(result.errors.end(), 
            forbiddenCheck.errors.begin(), forbiddenCheck.errors.end());
    }
    result.warnings.insert(result.warnings.end(),
        forbiddenCheck.warnings.begin(), forbiddenCheck.warnings.end());
    
    return result;
}

ValidationResult Sandbox::check_forbidden_calls(const std::string& script) {
    ValidationResult result;
    result.valid = true;
    
    for (const auto& forbidden : kForbiddenFunctions) {
        // Pattern: word boundary + forbidden name + optional dot or parenthesis
        std::string pattern = "\\b" + forbidden + "\\s*[.:(]";
        std::regex re(pattern);
        
        if (std::regex_search(script, re)) {
            result.valid = false;
            result.errors.push_back("Forbidden function/module used: " + forbidden);
        }
    }
    
    // Check for potential bytecode loading (binary strings)
    if (script.find("\\x1b") != std::string::npos || 
        script.find("\x1bLua") != std::string::npos ||
        script.find("\\27Lua") != std::string::npos) {
        result.valid = false;
        result.errors.push_back("Potential bytecode detected (security risk)");
    }
    
    // Warnings for common mistakes
    if (script.find("while true do") != std::string::npos ||
        script.find("while(true)") != std::string::npos) {
        result.warnings.push_back("Infinite loop detected - ensure proper exit condition");
    }
    
    return result;
}

std::unique_ptr<LuaState> Sandbox::create(const SandboxConfig& config) {
    auto state = std::make_unique<LuaState>();
    if (!state->init()) {
        return nullptr;
    }
    
    state->apply_sandbox(config.to_script_limits());
    
    // Setup custom print handler if provided
    if (config.printHandler) {
        auto handler = config.printHandler;
        state->state()["print"] = [handler](sol::variadic_args va, sol::this_state ts) {
            std::ostringstream oss;
            bool first = true;
            for (auto v : va) {
                if (!first) oss << "\t";
                first = false;
                
                sol::state_view lua(ts);
                sol::protected_function tostring = lua["tostring"];
                if (tostring.valid()) {
                    auto result = tostring(v);
                    if (result.valid()) {
                        oss << result.get<std::string>();
                    }
                }
            }
            handler(oss.str());
        };
    }
    
    return state;
}

const std::vector<std::string>& Sandbox::forbidden_functions() {
    return kForbiddenFunctions;
}

const std::vector<std::string>& Sandbox::safe_functions() {
    return kSafeFunctions;
}

// ScriptValidator implementation

ScriptValidator::ScriptValidator() {
    // Add default forbidden patterns
    for (const auto& func : kForbiddenFunctions) {
        add_forbidden_pattern("\\b" + func + "\\b");
    }
}

void ScriptValidator::add_forbidden_pattern(const std::string& pattern) {
    forbiddenPatterns_.push_back(pattern);
}

void ScriptValidator::add_warning_pattern(const std::string& pattern, const std::string& message) {
    warningPatterns_.emplace_back(pattern, message);
}

ValidationResult ScriptValidator::validate(const std::string& script) const {
    ValidationResult result;
    result.valid = true;
    
    // Check forbidden patterns
    for (const auto& pattern : forbiddenPatterns_) {
        try {
            std::regex re(pattern);
            std::smatch match;
            if (std::regex_search(script, match, re)) {
                result.valid = false;
                result.errors.push_back("Forbidden pattern found: " + match.str());
            }
        } catch (const std::regex_error&) {
            // Skip invalid patterns
        }
    }
    
    // Check warning patterns
    for (const auto& [pattern, message] : warningPatterns_) {
        try {
            std::regex re(pattern);
            if (std::regex_search(script, re)) {
                result.warnings.push_back(message);
            }
        } catch (const std::regex_error&) {
            // Skip invalid patterns
        }
    }
    
    return result;
}

} // namespace engine::scripting
