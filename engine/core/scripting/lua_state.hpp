#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <optional>
#include <vector>

// Forward declare sol types to avoid header pollution
struct lua_State;

namespace sol {
class state;
struct protected_function_result;  // sol2 declares this as struct
}

namespace engine::scripting {

// Script execution result
struct ScriptResult {
    bool success{false};
    std::string error;
    
    static ScriptResult ok() { return {true, ""}; }
    static ScriptResult fail(const std::string& err) { return {false, err}; }
    
    explicit operator bool() const { return success; }
};

// Memory and instruction limits for sandboxed scripts
struct ScriptLimits {
    std::size_t maxMemoryBytes{64 * 1024 * 1024};  // 64 MB default
    std::size_t maxInstructions{10000000};          // 10M instructions per call
    double maxExecutionTimeSec{5.0};                // 5 seconds max
};

// Lua VM wrapper with optional sandboxing
class LuaState {
public:
    LuaState();
    ~LuaState();
    
    // Non-copyable, movable
    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;
    LuaState(LuaState&&) noexcept;
    LuaState& operator=(LuaState&&) noexcept;
    
    // Initialize the Lua state
    bool init();
    
    // Apply sandbox restrictions (removes dangerous functions)
    void apply_sandbox(const ScriptLimits& limits = {});
    
    // Check if sandbox is active
    bool is_sandboxed() const { return sandboxed_; }
    
    // Load and execute a script string
    ScriptResult execute(const std::string& script, const std::string& chunkName = "script");
    
    // Load a script without executing (for syntax checking)
    ScriptResult load(const std::string& script, const std::string& chunkName = "script");
    
    // Call a global function by name (various overloads)
    ScriptResult call(const std::string& funcName);
    ScriptResult call(const std::string& funcName, int arg1);
    ScriptResult call(const std::string& funcName, float arg1);
    ScriptResult call(const std::string& funcName, const std::string& arg1);
    ScriptResult call(const std::string& funcName, std::uint32_t arg1);
    ScriptResult call(const std::string& funcName, std::uint32_t arg1, int arg2, int arg3, int arg4);
    ScriptResult call(const std::string& funcName, std::uint32_t arg1, int arg2, int arg3, int arg4, int arg5);
    
    // Check if a global function exists
    bool has_function(const std::string& funcName) const;
    
    // Set a global variable (various overloads)
    void set_global(const std::string& name, int value);
    void set_global(const std::string& name, float value);
    void set_global(const std::string& name, double value);
    void set_global(const std::string& name, bool value);
    void set_global(const std::string& name, const std::string& value);
    
    // Get a global variable (various overloads)
    std::optional<int> get_global_int(const std::string& name) const;
    std::optional<float> get_global_float(const std::string& name) const;
    std::optional<double> get_global_double(const std::string& name) const;
    std::optional<bool> get_global_bool(const std::string& name) const;
    std::optional<std::string> get_global_string(const std::string& name) const;
    
    // Access the underlying sol::state (for advanced usage)
    sol::state& state();
    const sol::state& state() const;
    
    // Get raw lua_State pointer (for C API interop)
    lua_State* lua_state();
    
    // Memory usage tracking
    std::size_t memory_used() const;
    
    // Reset the state (clear all globals, keep sandbox)
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool sandboxed_{false};
    ScriptLimits limits_{};
};

// Convenience function to create a sandboxed Lua state
std::unique_ptr<LuaState> create_sandboxed_state(const ScriptLimits& limits = {});

// Convenience function to create an unrestricted Lua state (for engine scripts)
std::unique_ptr<LuaState> create_engine_state();

} // namespace engine::scripting
