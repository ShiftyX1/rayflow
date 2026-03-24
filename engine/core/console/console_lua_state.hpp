#pragma once

// =============================================================================
// console_lua_state.hpp — Sandboxed Lua state for the in-game console.
//
// Uses a sandboxed LuaState (no io, os, debug, loadfile, require, etc.)
// with a curated API surface: engine.*, math/vec2/vec3, events.*, timer.*,
// cvar.*, plus game-extensible modules via register_api() / register_api_module().
//
// Supports:
//  - exec(path)           — load user scripts from a whitelisted directory
//  - autoexec.lua         — executed automatically on init
//  - ConVar integration   — cvar.get/set/list/reset/describe
//  - Timer ticks          — update(dt) each frame
//  - Game API extension   — register_api(), register_api_module()
// =============================================================================

#include "engine/core/export.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/forward.hpp>

namespace engine::console {

class ConsoleLogSink;
class ConVarRegistry;

class RAYFLOW_CORE_API ConsoleLuaState {
public:
    ConsoleLuaState();
    ~ConsoleLuaState();

    ConsoleLuaState(const ConsoleLuaState&) = delete;
    ConsoleLuaState& operator=(const ConsoleLuaState&) = delete;

    /// Initialise the Lua state with sandbox and register all APIs.
    /// @param sink            Console log sink — log/print output goes here.
    /// @param userScriptsDir  Whitelisted directory for exec() script loading.
    ///                        Pass empty path to disable exec().
    /// @param cvars           Optional ConVar registry for cvar.* API.
    bool init(ConsoleLogSink& sink,
              const std::filesystem::path& userScriptsDir = {},
              ConVarRegistry* cvars = nullptr);

    /// Execute a single command string (typed by the user).
    /// Returns the printed / returned result, or an error string.
    std::string execute(std::string_view command);

    /// Tick timers — call once per frame.
    void update(float dt);

    /// Allow games to extend the console API.
    /// The callback receives the underlying sol::state.
    void register_api(std::function<void(sol::state&)> registrar);

    /// Register a named API module (namespace table).
    /// Matches ScriptEngineBase::register_api_module() pattern.
    void register_api_module(const std::string& namespaceName,
                             std::function<void(sol::state&, sol::table&)> registrar);

    /// List registered API module namespace names.
    std::vector<std::string> list_api_modules() const;

    /// Raw access for advanced usage.
    sol::state& state();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace engine::console
