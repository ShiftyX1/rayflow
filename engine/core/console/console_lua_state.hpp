#pragma once

// =============================================================================
// console_lua_state.hpp — Unsandboxed Lua state for the developer console.
//
// Wraps LuaState with full library access (io, os, debug) and routes
// engine.log() output into ConsoleLogSink.  Games can extend the console
// API via register_api().
// =============================================================================

#include "engine/core/export.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace sol { class state; }

namespace engine::console {

class ConsoleLogSink;

class RAYFLOW_CORE_API ConsoleLuaState {
public:
    ConsoleLuaState();
    ~ConsoleLuaState();

    ConsoleLuaState(const ConsoleLuaState&) = delete;
    ConsoleLuaState& operator=(const ConsoleLuaState&) = delete;

    /// Initialise the Lua state and register base APIs.
    /// @param sink  Console log sink — engine.log() output goes here.
    bool init(ConsoleLogSink& sink);

    /// Execute a single command string.
    /// Returns the printed / returned result, or an error string.
    std::string execute(std::string_view command);

    /// Allow games to extend the console API.
    /// The callback receives the underlying sol::state.
    void register_api(std::function<void(sol::state&)> registrar);

    /// Raw access for advanced usage.
    sol::state& state();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace engine::console
