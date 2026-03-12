-- =============================================================================
-- BedWars Client Game Scripts — Entry Point
-- =============================================================================
-- Client-side game scripts for visual effects, HUD logic, and client-side
-- prediction. These run in a separate Lua VM from server scripts.
--
-- Available API (to be expanded):
--   log(message)  — Print to client console
--
-- Client hooks:
--   on_game_init()              — Called once when scripts are loaded
-- =============================================================================

log("[game scripts] BedWars client scripts loaded")

function test_function()
    for i = 1, 5 do
        log("Test function iteration " .. i)
    end
end

function on_game_init()
    log("[game scripts] Client game scripts initialized")
    test_function()
end
