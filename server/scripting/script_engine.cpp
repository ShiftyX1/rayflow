#include "script_engine.hpp"
#include "game_api.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <algorithm>

namespace server::scripting {

ScriptEngine::ScriptEngine() = default;
ScriptEngine::~ScriptEngine() = default;

bool ScriptEngine::init() {
    auto config = shared::scripting::SandboxConfig::default_for_maps();
    
    // Set up print handler to route to our log callback
    config.printHandler = [this](const std::string& msg) {
        if (logCallback_) {
            logCallback_(msg);
        }
    };
    
    lua_ = shared::scripting::Sandbox::create(config);
    if (!lua_) {
        lastError_ = "Failed to create sandboxed Lua state";
        return false;
    }
    
    api_ = std::make_unique<GameAPI>(*this);
    setup_game_api();
    
    return true;
}

void ScriptEngine::setup_game_api() {
    if (!lua_ || !api_) return;
    
    api_->register_api(lua_->state());
}

shared::scripting::ScriptResult ScriptEngine::load_map_scripts(
    const shared::scripting::MapScriptData& scripts) {
    
    if (!lua_) {
        return shared::scripting::ScriptResult::fail("Engine not initialized");
    }
    
    unload();
    
    if (scripts.empty()) {
        return shared::scripting::ScriptResult::ok();
    }
    
    // Validate scripts first
    auto validation = shared::scripting::Sandbox::validate_script(scripts.mainScript);
    if (!validation.valid) {
        std::string errors;
        for (const auto& err : validation.errors) {
            if (!errors.empty()) errors += "; ";
            errors += err;
        }
        lastError_ = "Script validation failed: " + errors;
        return shared::scripting::ScriptResult::fail(lastError_);
    }
    
    // Load modules first (they might be required by main script)
    for (const auto& mod : scripts.modules) {
        // Validate module
        auto modValidation = shared::scripting::Sandbox::validate_script(mod.content);
        if (!modValidation.valid) {
            lastError_ = "Module '" + mod.name + "' validation failed";
            return shared::scripting::ScriptResult::fail(lastError_);
        }
        
        // Execute module to register it
        auto result = lua_->execute(mod.content, mod.name);
        if (!result) {
            lastError_ = "Failed to load module '" + mod.name + "': " + result.error;
            return shared::scripting::ScriptResult::fail(lastError_);
        }
    }
    
    // Execute main script
    auto result = lua_->execute(scripts.mainScript, "main.lua");
    if (!result) {
        lastError_ = "Failed to load main script: " + result.error;
        return shared::scripting::ScriptResult::fail(lastError_);
    }
    
    scriptsLoaded_ = true;
    
    // Call init hook if present
    if (lua_->has_function("on_init")) {
        call_hook("on_init");
    }
    
    return shared::scripting::ScriptResult::ok();
}

void ScriptEngine::unload() {
    if (!lua_) return;
    
    // Call cleanup hook if present
    if (scriptsLoaded_ && lua_->has_function("on_unload")) {
        call_hook("on_unload");
    }
    
    scriptsLoaded_ = false;
    timers_.clear();
    pendingCommands_.clear();
    
    // Reset Lua state but keep sandbox
    lua_->reset();
    setup_game_api();
}

void ScriptEngine::update(float deltaTime) {
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

std::vector<ScriptCommand> ScriptEngine::take_commands() {
    std::vector<ScriptCommand> result;
    result.swap(pendingCommands_);
    return result;
}

void ScriptEngine::queue_command(ScriptCommand cmd) {
    pendingCommands_.push_back(std::move(cmd));
}

void ScriptEngine::add_timer(const std::string& name, double delaySec, 
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

void ScriptEngine::cancel_timer(const std::string& name) {
    for (auto& timer : timers_) {
        if (timer.name == name) {
            timer.cancelled = true;
        }
    }
}

void ScriptEngine::call_hook(const char* hookName) {
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

// Event implementations
void ScriptEngine::on_player_join(std::uint32_t playerId) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_player_join")) {
        auto result = lua_->call("on_player_join", playerId);
        if (!result && logCallback_) {
            logCallback_("[script error] on_player_join: " + result.error);
        }
    }
}

void ScriptEngine::on_player_leave(std::uint32_t playerId) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_player_leave")) {
        auto result = lua_->call("on_player_leave", playerId);
        if (!result && logCallback_) {
            logCallback_("[script error] on_player_leave: " + result.error);
        }
    }
}

void ScriptEngine::on_player_spawn(std::uint32_t playerId, float x, float y, float z) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_player_spawn")) {
        lua_->state()["on_player_spawn"](playerId, x, y, z);
    }
}

void ScriptEngine::on_player_death(std::uint32_t playerId, std::uint32_t killerId) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_player_death")) {
        lua_->state()["on_player_death"](playerId, killerId);
    }
}

void ScriptEngine::on_block_break(std::uint32_t playerId, int x, int y, int z, int blockType) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_block_break")) {
        auto result = lua_->call("on_block_break", playerId, x, y, z, blockType);
        if (!result && logCallback_) {
            logCallback_("[script error] on_block_break: " + result.error);
        }
    }
}

void ScriptEngine::on_block_place(std::uint32_t playerId, int x, int y, int z, int blockType) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_block_place")) {
        auto result = lua_->call("on_block_place", playerId, x, y, z, blockType);
        if (!result && logCallback_) {
            logCallback_("[script error] on_block_place: " + result.error);
        }
    }
}

void ScriptEngine::on_round_start(int roundNumber) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_round_start")) {
        auto result = lua_->call("on_round_start", roundNumber);
        if (!result && logCallback_) {
            logCallback_("[script error] on_round_start: " + result.error);
        }
    }
}

void ScriptEngine::on_round_end(int winningTeam) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_round_end")) {
        auto result = lua_->call("on_round_end", winningTeam);
        if (!result && logCallback_) {
            logCallback_("[script error] on_round_end: " + result.error);
        }
    }
}

void ScriptEngine::on_match_start() {
    call_hook("on_match_start");
}

void ScriptEngine::on_match_end(int winningTeam) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_match_end")) {
        auto result = lua_->call("on_match_end", winningTeam);
        if (!result && logCallback_) {
            logCallback_("[script error] on_match_end: " + result.error);
        }
    }
}

void ScriptEngine::on_custom_event(const std::string& eventName, const std::string& data) {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_custom")) {
        lua_->state()["on_custom"](eventName, data);
    }
}

void ScriptEngine::set_log_callback(std::function<void(const std::string&)> callback) {
    logCallback_ = std::move(callback);
}

} // namespace server::scripting

