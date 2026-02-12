#pragma once

// =============================================================================
// Engine API Module — generic engine functions available to all Lua scripts
// Provides: engine.log(...), engine.time(), engine.delta_time(), engine.version()
// =============================================================================

#include <functional>
#include <string>

namespace sol { class state; }

namespace engine::scripting::api {

/// Register the "engine" namespace in Lua:
///   engine.log(...)        — formatted logging
///   engine.time()          — elapsed time since engine start (seconds)
///   engine.delta_time()    — last frame/tick delta time
///   engine.version()       — engine version string
///
/// @param lua        The sol::state to register into
/// @param logCallback  Callback for log output (may be nullptr)
/// @param getDeltaTime Callback returning current delta time
void register_engine_api(sol::state& lua,
                         std::function<void(const std::string&)> logCallback,
                         std::function<float()> getDeltaTime);

} // namespace engine::scripting::api
