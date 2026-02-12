#pragma once

// =============================================================================
// Timer API Module — unified timer management for Lua scripts
// Provides: timer.after(delay, callback), timer.every(interval, callback),
//           timer.cancel(name)
// =============================================================================

#include <functional>
#include <string>

namespace sol { class state; }

namespace engine::scripting {
class ScriptEngineBase;
}

namespace engine::scripting::api {

/// Register the "timer" namespace in Lua:
///   timer.after(delay, callback)            — one-shot timer
///   timer.every(interval, callback)         — repeating timer
///   timer.named(name, delay, callback)      — named one-shot timer
///   timer.cancel(name)                      — cancel named timer
///
/// @param lua     The sol::state to register into
/// @param engine  The ScriptEngineBase that owns the timer list
void register_timer_api(sol::state& lua, ScriptEngineBase& engine);

} // namespace engine::scripting::api
