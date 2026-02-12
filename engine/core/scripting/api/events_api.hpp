#pragma once

// =============================================================================
// Events API Module — generic event bus for Lua scripts
// Provides: events.on(name, callback [, priority]),
//           events.emit(name, ...),
//           events.off(name, callback)
// =============================================================================

namespace sol { class state; }

namespace engine::scripting::api {

/// Register the "events" namespace in Lua:
///   events.on(name, callback [, priority])  — subscribe to event
///   events.emit(name, ...)                  — fire event to all listeners
///   events.off(name)                        — unsubscribe all from event
///   events.clear()                          — remove all listeners
///
/// Priority: lower = called first. Default = 100.
/// If a listener returns false, the event is cancelled (remaining listeners skipped).
void register_events_api(sol::state& lua);

} // namespace engine::scripting::api
