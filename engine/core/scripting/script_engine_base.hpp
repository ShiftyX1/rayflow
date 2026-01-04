#pragma once

#include "lua_state.hpp"
#include "sandbox.hpp"
#include "script_types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace engine::scripting {

// Timer entry for script timers
struct ScriptTimer {
    std::string name;
    double remainingSec{0.0};
    double intervalSec{0.0};  // 0 = one-shot, >0 = repeating
    std::string callbackFunc;
    bool cancelled{false};
};

// Base class for game-specific script engines.
// Provides common infrastructure for Lua scripting with sandboxing.
// Games inherit and implement register_game_api() to expose their specific API.
class ScriptEngineBase {
public:
    ScriptEngineBase();
    virtual ~ScriptEngineBase();
    
    // Non-copyable
    ScriptEngineBase(const ScriptEngineBase&) = delete;
    ScriptEngineBase& operator=(const ScriptEngineBase&) = delete;
    
    // Initialize the engine with sandbox configuration
    bool init(const SandboxConfig& config = SandboxConfig::default_for_maps());
    
    // Load scripts from MapScriptData
    ScriptResult load_map_scripts(const MapScriptData& scripts);
    
    // Unload current scripts
    void unload();
    
    // Check if scripts are loaded
    bool has_scripts() const { return scriptsLoaded_; }
    
    // Update timers (call every frame/tick)
    void update(float deltaTime);
    
    // Access to underlying Lua state (for advanced usage)
    LuaState* lua_state() { return lua_.get(); }
    const LuaState* lua_state() const { return lua_.get(); }
    
    // Get last error message
    const std::string& last_error() const { return lastError_; }
    
    // Set logging callback for script print() calls
    void set_log_callback(std::function<void(const std::string&)> callback);

protected:
    // Override this to register game-specific API
    // Called after engine initialization
    virtual void register_game_api(LuaState& lua) = 0;
    
    // Override this to setup game-specific constants (BLOCK.*, TEAM.*, etc.)
    virtual void register_constants(LuaState& lua) {}
    
    // Call a Lua hook/callback by name (no args)
    void call_hook(const char* hookName);
    
    // Timer management (can be exposed via API in derived class)
    void add_timer(const std::string& name, double delaySec, 
                   double intervalSec, const std::string& callback);
    void cancel_timer(const std::string& name);
    
    // Access for derived classes
    std::function<void(const std::string&)>& log_callback() { return logCallback_; }

private:
    void setup_base_api();
    
    std::unique_ptr<LuaState> lua_;
    
    bool scriptsLoaded_{false};
    std::string lastError_;
    
    std::vector<ScriptTimer> timers_;
    
    std::function<void(const std::string&)> logCallback_;
};

} // namespace engine::scripting
