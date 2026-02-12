#include "script_engine_base.hpp"
#include "engine/vfs/vfs.hpp"

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
    gameScriptsLoaded_ = false;
    gameScriptsBasePath_.clear();
    timers_.clear();
    
    // Reset Lua state but keep sandbox
    lua_->reset();
    setup_base_api();
    register_constants(*lua_);
    register_game_api(*lua_);
}

void ScriptEngineBase::unload_map_scripts() {
    if (!lua_) return;
    
    // Call map-specific cleanup hook
    if (scriptsLoaded_ && lua_->has_function("on_map_unload")) {
        call_hook("on_map_unload");
    }
    
    // Only reset map scripts flag, keep game scripts loaded
    scriptsLoaded_ = false;
    timers_.clear();
    
    // If game scripts are loaded, we need to reset and reload them
    // (simpler than trying to selectively unload map-scope globals)
    if (gameScriptsLoaded_) {
        std::string basePath = gameScriptsBasePath_;
        
        lua_->reset();
        setup_base_api();
        register_constants(*lua_);
        register_game_api(*lua_);
        
        // Reload game scripts
        auto result = load_game_scripts(basePath);
        if (!result) {
            if (logCallback_) {
                logCallback_("[script error] Failed to reload game scripts: " + result.error);
            }
        }
    }
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
