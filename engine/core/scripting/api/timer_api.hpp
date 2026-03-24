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

/// Generic timer host interface — anything that manages a list of timers.
/// Both ScriptEngineBase and ConsoleLuaState implement this.
struct ITimerHost {
    std::function<void(const std::string& name, double delaySec,
                       double intervalSec, const std::string& callback)> add_timer;
    std::function<void(const std::string& name)> cancel_timer;
};

/// Register the "timer" namespace in Lua:
///   timer.after(delay, callback)            — one-shot timer
///   timer.every(interval, callback)         — repeating timer
///   timer.named(name, delay, callback)      — named one-shot timer
///   timer.cancel(name)                      — cancel named timer
///
/// @param lua     The sol::state to register into
/// @param engine  The ScriptEngineBase that owns the timer list
void register_timer_api(sol::state& lua, ScriptEngineBase& engine);

/// Overload accepting a generic timer host (for use by ConsoleLuaState etc.)
void register_timer_api(sol::state& lua, ITimerHost& host);

} // namespace engine::scripting::api
