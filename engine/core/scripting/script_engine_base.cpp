#include "script_engine_base.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <algorithm>

namespace engine::scripting {

ScriptEngineBase::ScriptEngineBase() = default;
ScriptEngineBase::~ScriptEngineBase() = default;

bool ScriptEngineBase::init(const SandboxConfig& config) {
    // Set up print handler to route to our log callback
    SandboxConfig cfg = config;
    cfg.printHandler = [this](const std::string& msg) {
        if (logCallback_) {
            logCallback_(msg);
        }
    };
    
    lua_ = Sandbox::create(cfg);
    if (!lua_) {
        lastError_ = "Failed to create sandboxed Lua state";
        return false;
    }
    
    setup_base_api();
    register_constants(*lua_);
    register_game_api(*lua_);
    
    return true;
}

void ScriptEngineBase::setup_base_api() {
    if (!lua_) return;
    
    // Base API available to all games:
    // - log() function (alias for print)
    // - time() function (server time)
    
    auto& state = lua_->state();
    
    // Provide log as alias for print
    state["log"] = state["print"];
}

ScriptResult ScriptEngineBase::load_map_scripts(const MapScriptData& scripts) {
    if (!lua_) {
        return ScriptResult::fail("Engine not initialized");
    }
    
    unload();
    
    if (scripts.empty()) {
        return ScriptResult::ok();
    }
    
    // Validate scripts first
    auto validation = Sandbox::validate_script(scripts.mainScript);
    if (!validation.valid) {
        std::string errors;
        for (const auto& err : validation.errors) {
            if (!errors.empty()) errors += "; ";
            errors += err;
        }
        lastError_ = "Script validation failed: " + errors;
        return ScriptResult::fail(lastError_);
    }
    
    // Load modules first (they might be required by main script)
    for (const auto& mod : scripts.modules) {
        // Validate module
        auto modValidation = Sandbox::validate_script(mod.content);
        if (!modValidation.valid) {
            lastError_ = "Module '" + mod.name + "' validation failed";
            return ScriptResult::fail(lastError_);
        }
        
        // Execute module to register it
        auto result = lua_->execute(mod.content, mod.name);
        if (!result) {
            lastError_ = "Failed to load module '" + mod.name + "': " + result.error;
            return ScriptResult::fail(lastError_);
        }
    }
    
    // Execute main script
    auto result = lua_->execute(scripts.mainScript, "main.lua");
    if (!result) {
        lastError_ = "Failed to load main script: " + result.error;
        return ScriptResult::fail(lastError_);
    }
    
    scriptsLoaded_ = true;
    
    // Call init hook if present
    if (lua_->has_function("on_init")) {
        call_hook("on_init");
    }
    
    return ScriptResult::ok();
}

void ScriptEngineBase::unload() {
    if (!lua_) return;
    
    // Call cleanup hook if present
    if (scriptsLoaded_ && lua_->has_function("on_unload")) {
        call_hook("on_unload");
    }
    
    scriptsLoaded_ = false;
    timers_.clear();
    
    // Reset Lua state but keep sandbox
    lua_->reset();
    setup_base_api();
    register_constants(*lua_);
    register_game_api(*lua_);
}

void ScriptEngineBase::update(float deltaTime) {
    if (!scriptsLoaded_) return;
    
    // Update timers
    for (auto& timer : timers_) {
        if (timer.cancelled) continue;
        
        timer.remainingSec -= static_cast<double>(deltaTime);
        
        if (timer.remainingSec <= 0.0) {
            // Fire callback
            if (lua_->has_function(timer.callbackFunc)) {
                call_hook(timer.callbackFunc.c_str());
            }
            
            if (timer.intervalSec > 0.0) {
                // Repeating timer - reset
                timer.remainingSec = timer.intervalSec;
            } else {
                // One-shot timer - mark as cancelled
                timer.cancelled = true;
            }
        }
    }
    
    // Remove cancelled timers
    timers_.erase(
        std::remove_if(timers_.begin(), timers_.end(),
            [](const ScriptTimer& t) { return t.cancelled; }),
        timers_.end());
    
    // Call update hook if present
    if (lua_->has_function("on_update")) {
        lua_->call("on_update", deltaTime);
    }
}

void ScriptEngineBase::add_timer(const std::string& name, double delaySec, 
                                  double intervalSec, const std::string& callback) {
    // Cancel existing timer with same name
    cancel_timer(name);
    
    ScriptTimer timer;
    timer.name = name;
    timer.remainingSec = delaySec;
    timer.intervalSec = intervalSec;
    timer.callbackFunc = callback;
    timer.cancelled = false;
    
    timers_.push_back(std::move(timer));
}

void ScriptEngineBase::cancel_timer(const std::string& name) {
    for (auto& timer : timers_) {
        if (timer.name == name) {
            timer.cancelled = true;
        }
    }
}

void ScriptEngineBase::call_hook(const char* hookName) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function(hookName)) {
        auto result = lua_->call(hookName);
        if (!result) {
            lastError_ = std::string("Hook '") + hookName + "' error: " + result.error;
            if (logCallback_) {
                logCallback_("[script error] " + lastError_);
            }
        }
    }
}

void ScriptEngineBase::set_log_callback(std::function<void(const std::string&)> callback) {
    logCallback_ = std::move(callback);
}

} // namespace engine::scripting
