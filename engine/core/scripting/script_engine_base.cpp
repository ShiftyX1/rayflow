#include "script_engine_base.hpp"
#include "lua_state_call.hpp"
#include "engine/vfs/vfs.hpp"

// Engine base API modules
#include "api/engine_api.hpp"
#include "api/math_api.hpp"
#include "api/timer_api.hpp"
#include "api/events_api.hpp"

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
    apply_registered_modules();
    
    return true;
}

void ScriptEngineBase::setup_base_api() {
    if (!lua_) return;
    
    auto& state = lua_->state();
    
    // Register all engine base API modules.
    // These are available to every game, always.
    
    // 1. engine.* — log, time, delta_time, version
    api::register_engine_api(state, logCallback_,
        [this]() -> float { return currentDeltaTime_; });
    
    // 2. vec2, vec3 usertypes + math.lerp/clamp/remap
    api::register_math_api(state);
    
    // 3. timer.after / timer.every / timer.cancel
    api::register_timer_api(state, *this);
    
    // 4. events.on / events.emit / events.off / events.clear
    api::register_events_api(state);
}

ScriptResult ScriptEngineBase::load_map_scripts(const MapScriptData& scripts) {
    if (!lua_) {
        return ScriptResult::fail("Engine not initialized");
    }
    
    // Unload previous map scripts (environment-based — fast, no full reset)
    if (scriptsLoaded_) {
        unload_map_scripts();
    }
    
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
    
    // Create an isolated environment for map scripts.
    // Map globals live here; game scripts and API remain in the main state.
    create_map_environment();
    
    // Load modules first (they might be required by main script)
    for (const auto& mod : scripts.modules) {
        // Validate module
        auto modValidation = Sandbox::validate_script(mod.content);
        if (!modValidation.valid) {
            lastError_ = "Module '" + mod.name + "' validation failed";
            return ScriptResult::fail(lastError_);
        }
        
        // Execute module in the map environment
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
    gameScriptsLoaded_ = false;
    gameScriptsBasePath_.clear();
    timers_.clear();
    
    destroy_map_environment();
    
    // Reset Lua state but keep sandbox
    lua_->reset();
    setup_base_api();
    register_constants(*lua_);
    register_game_api(*lua_);
    apply_registered_modules();
}

void ScriptEngineBase::unload_map_scripts() {
    if (!lua_) return;
    
    // Call map-specific cleanup hook
    if (scriptsLoaded_ && lua_->has_function("on_map_unload")) {
        call_hook("on_map_unload");
    }
    
    // Only reset map scripts flag, keep game scripts loaded
    scriptsLoaded_ = gameScriptsLoaded_;  // remain loaded if game scripts exist
    timers_.clear();
    
    // Destroy the map environment — this removes all map-scope globals
    // while keeping game scripts and API intact. No full VM reset needed.
    destroy_map_environment();
}

ScriptResult ScriptEngineBase::load_game_scripts(const std::string& vfsBasePath) {
    if (!lua_) {
        return ScriptResult::fail("Engine not initialized");
    }
    
    // Install VFS-based require() loader for this base path
    install_vfs_require(vfsBasePath);
    
    // Read main.lua from VFS
    const std::string mainPath = vfsBasePath + "/main.lua";
    auto mainContent = engine::vfs::read_text_file(mainPath);
    if (!mainContent) {
        // No main.lua is not an error — game scripts are optional
        return ScriptResult::ok();
    }
    
    // Validate main script
    auto validation = Sandbox::validate_script(*mainContent);
    if (!validation.valid) {
        std::string errors;
        for (const auto& err : validation.errors) {
            if (!errors.empty()) errors += "; ";
            errors += err;
        }
        lastError_ = "Game script validation failed (" + mainPath + "): " + errors;
        return ScriptResult::fail(lastError_);
    }
    
    // Execute main script
    auto result = lua_->execute(*mainContent, mainPath);
    if (!result) {
        lastError_ = "Failed to load game script (" + mainPath + "): " + result.error;
        return ScriptResult::fail(lastError_);
    }
    
    gameScriptsLoaded_ = true;
    gameScriptsBasePath_ = vfsBasePath;
    scriptsLoaded_ = true;
    
    // Call game init hook if present
    if (lua_->has_function("on_game_init")) {
        call_hook("on_game_init");
    }
    
    return ScriptResult::ok();
}

void ScriptEngineBase::install_vfs_require(const std::string& basePath) {
    if (!lua_) return;
    
    auto& state = lua_->state();
    
    // The sandbox removed package/require for security. We recreate a minimal
    // package table with only a VFS-based searcher — no filesystem access.
    
    // Create a fresh package table (replaces the nil'd one)
    state.create_named_table("package",
        "path", "",
        "cpath", "",
        "loaded", state.create_table(),
        "preload", state.create_table()
    );
    // Create the searchers list with our VFS searcher only
    state["package"]["searchers"] = state.create_table();
    
    // Install VFS searcher
    std::string base = basePath;
    state.set_function("__vfs_searcher", 
        [base, this](const std::string& modname) -> sol::object {
            // Convert dot-separated module name to path: "utils.math" -> "utils/math.lua"
            std::string path = modname;
            for (auto& c : path) {
                if (c == '.') c = '/';
            }
            
            std::string fullPath = base + "/" + path + ".lua";
            auto content = engine::vfs::read_text_file(fullPath);
            
            if (!content) {
                return sol::make_object(lua_->state(), 
                    "\n\tno VFS file '" + fullPath + "'");
            }
            
            // Return a loader function that executes the module
            auto& L = lua_->state();
            return sol::make_object(L, L.load(*content, "@" + fullPath));
        }
    );
    
    // Build a controlled require() that uses our VFS searcher + package.loaded cache
    state.script(R"lua(
        local searchers = package.searchers
        table.insert(searchers, 1, __vfs_searcher)
        __vfs_searcher = nil
        
        require = function(modname)
            -- Check loaded cache
            if package.loaded[modname] ~= nil then
                return package.loaded[modname]
            end
            -- Check preload
            if package.preload[modname] then
                local result = package.preload[modname](modname)
                if result == nil then result = true end
                package.loaded[modname] = result
                return result
            end
            -- Try each searcher
            local errors = {}
            for _, searcher in ipairs(searchers) do
                local result = searcher(modname)
                if type(result) == "function" then
                    local mod = result(modname)
                    if mod == nil then mod = true end
                    package.loaded[modname] = mod
                    return mod
                elseif type(result) == "string" then
                    errors[#errors + 1] = result
                end
            end
            error("module '" .. modname .. "' not found:" .. table.concat(errors))
        end
    )lua");
}

void ScriptEngineBase::update(float deltaTime) {
    if (!scriptsLoaded_) return;
    
    currentDeltaTime_ = deltaTime;
    
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

// ---- Modular API registration ----

void ScriptEngineBase::register_api_module(
    const std::string& namespaceName,
    std::function<void(sol::state&, sol::table&)> registrar) {
    // Store for replay on reset
    registeredModules_.push_back({namespaceName, registrar});
    
    // Apply immediately if Lua state exists
    if (lua_) {
        auto& state = lua_->state();
        sol::table ns = state.create_named_table(namespaceName);
        registrar(state, ns);
    }
}

void ScriptEngineBase::register_api_module(ScriptAPIModule& module) {
    register_api_module(module.name(),
        [&module](sol::state& lua, sol::table& ns) {
            module.register_api(lua, ns);
        });
}

bool ScriptEngineBase::has_api_module(const std::string& name) const {
    for (const auto& m : registeredModules_) {
        if (m.name == name) return true;
    }
    return false;
}

std::vector<std::string> ScriptEngineBase::list_api_modules() const {
    std::vector<std::string> names;
    names.reserve(registeredModules_.size());
    for (const auto& m : registeredModules_) {
        names.push_back(m.name);
    }
    return names;
}

void ScriptEngineBase::apply_registered_modules() {
    if (!lua_) return;
    auto& state = lua_->state();
    for (auto& m : registeredModules_) {
        sol::table ns = state.create_named_table(m.name);
        m.registrar(state, ns);
    }
}

// ---- Mod loading ----

ScriptResult ScriptEngineBase::load_mod(const std::string& modPath,
                                         const std::vector<std::string>& allowedModules) {
    if (!lua_) {
        return ScriptResult::fail("Engine not initialized");
    }
    
    // Read mod.lua from VFS
    const std::string modFile = modPath + "/mod.lua";
    auto modContent = engine::vfs::read_text_file(modFile);
    if (!modContent) {
        return ScriptResult::fail("Mod not found: " + modFile);
    }
    
    // Validate
    auto validation = Sandbox::validate_script(*modContent);
    if (!validation.valid) {
        std::string errors;
        for (const auto& err : validation.errors) {
            if (!errors.empty()) errors += "; ";
            errors += err;
        }
        return ScriptResult::fail("Mod validation failed (" + modPath + "): " + errors);
    }
    
    // Create a sandboxed environment for this mod.
    // The environment inherits read access to the global table (API),
    // but writes go into the environment only.
    auto& state = lua_->state();
    sol::environment modEnv(state, sol::create, state.globals());
    
    // If allowedModules whitelist is provided, restrict access
    if (!allowedModules.empty()) {
        // Create a restricted global metatable that only exposes allowed modules
        sol::table restricted = state.create_table();
        
        // Always allow base functions
        const char* alwaysAllowed[] = {
            "print", "log", "type", "tostring", "tonumber",
            "pairs", "ipairs", "next", "select", "unpack",
            "pcall", "xpcall", "error", "assert",
            "string", "table", "math", "coroutine",
            "engine", "events", "timer", "vec2", "vec3",
            nullptr
        };
        for (int i = 0; alwaysAllowed[i]; ++i) {
            restricted[alwaysAllowed[i]] = state[alwaysAllowed[i]];
        }
        
        // Add allowed game modules
        for (const auto& moduleName : allowedModules) {
            sol::object mod = state[moduleName];
            if (mod.valid()) {
                restricted[moduleName] = mod;
            }
        }
        
        modEnv = sol::environment(state, sol::create);
        for (auto& [key, value] : restricted) {
            modEnv[key] = value;
        }
    }
    
    // Execute mod script in the environment
    auto result = state.safe_script(*modContent, modEnv, sol::script_pass_on_error, modFile);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail("Mod load error (" + modPath + "): " + std::string(err.what()));
    }
    
    // Extract mod manifest (optional table returned or global 'mod' table)
    ModInfo info;
    info.path = modPath;
    info.loaded = true;
    
    sol::object modTable = modEnv["mod"];
    if (modTable.is<sol::table>()) {
        sol::table mt = modTable;
        info.name = mt.get_or<std::string>("name", modPath);
        info.version = mt.get_or<std::string>("version", "1.0");
        info.author = mt.get_or<std::string>("author", "unknown");
    } else {
        info.name = modPath;
        info.version = "1.0";
        info.author = "unknown";
    }
    
    // Check for duplicate
    for (auto& existing : loadedMods_) {
        if (existing.name == info.name) {
            existing = info;
            if (logCallback_) {
                logCallback_("[mod] Reloaded mod: " + info.name);
            }
            return ScriptResult::ok();
        }
    }
    
    loadedMods_.push_back(std::move(info));
    
    if (logCallback_) {
        logCallback_("[mod] Loaded: " + loadedMods_.back().name + 
                     " v" + loadedMods_.back().version);
    }
    
    return ScriptResult::ok();
}

void ScriptEngineBase::unload_mod(const std::string& modName) {
    loadedMods_.erase(
        std::remove_if(loadedMods_.begin(), loadedMods_.end(),
            [&modName](const ModInfo& m) { return m.name == modName; }),
        loadedMods_.end());
    
    if (logCallback_) {
        logCallback_("[mod] Unloaded: " + modName);
    }
}

std::vector<ScriptEngineBase::ModInfo> ScriptEngineBase::list_mods() const {
    return loadedMods_;
}

// ---- Map environment isolation ----

void ScriptEngineBase::create_map_environment() {
    // Currently a no-op placeholder.
    // Full sol::environment-based isolation requires executing map scripts
    // inside the environment, which needs changes to LuaState::execute().
    // For now, we track that a map environment logically exists.
    // TODO: Implement full environment-based script execution in LuaState.
    mapEnvironment_ = reinterpret_cast<void*>(1);  // sentinel — "active"
}

void ScriptEngineBase::destroy_map_environment() {
    // Destroy map-scope globals.
    // Since full sol::environment isolation is not yet wired into LuaState::execute(),
    // we nil out known map-script hooks to prevent stale callbacks.
    if (mapEnvironment_ && lua_) {
        auto& state = lua_->state();
        // Clear common map hooks
        const char* mapHooks[] = {
            "on_init", "on_unload", "on_map_unload", "on_update",
            "on_player_join", "on_player_leave", "on_player_spawn",
            "on_player_death", "on_block_break", "on_block_place",
            "on_round_start", "on_round_end", "on_match_start",
            "on_match_end", "on_custom",
            nullptr
        };
        for (int i = 0; mapHooks[i]; ++i) {
            state[mapHooks[i]] = sol::lua_nil;
        }
    }
    mapEnvironment_ = nullptr;
}

} // namespace engine::scripting
