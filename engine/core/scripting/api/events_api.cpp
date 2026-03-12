#include "events_api.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace engine::scripting::api {

void register_events_api(sol::state& lua) {
    // The event bus is implemented entirely in Lua for simplicity and
    // to avoid crossing the C++/Lua boundary on every emit.
    // C++ code can still emit events via state["events"]["emit"](name, ...).

    lua.script(R"lua(
        events = {}

        -- Internal storage: events._listeners[name] = {{fn=..., priority=...}, ...}
        events._listeners = {}

        --- Subscribe to an event.
        --- @param name     string   Event name
        --- @param callback function Listener function
        --- @param priority number   Lower = called first (default 100)
        function events.on(name, callback, priority)
            priority = priority or 100
            if not events._listeners[name] then
                events._listeners[name] = {}
            end
            local list = events._listeners[name]
            list[#list + 1] = { fn = callback, priority = priority }
            -- Sort by priority (stable-ish: Lua sort is not guaranteed stable,
            -- but for integer priorities it's fine)
            table.sort(list, function(a, b) return a.priority < b.priority end)
        end

        --- Emit an event. Calls all listeners in priority order.
        --- If any listener returns false, remaining listeners are skipped
        --- and emit returns false (event cancelled).
        --- @param name string Event name
        --- @return boolean true if not cancelled
        function events.emit(name, ...)
            local list = events._listeners[name]
            if not list then return true end
            for _, entry in ipairs(list) do
                local result = entry.fn(...)
                if result == false then
                    return false
                end
            end
            return true
        end

        --- Unsubscribe all listeners from an event.
        function events.off(name)
            events._listeners[name] = nil
        end

        --- Remove all listeners for all events.
        function events.clear()
            events._listeners = {}
        end
    )lua");
}

} // namespace engine::scripting::api
