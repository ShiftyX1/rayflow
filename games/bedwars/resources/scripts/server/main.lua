-- =============================================================================
-- BedWars Server Game Scripts — Entry Point
-- =============================================================================
-- This file is loaded once when the server starts and persists across map changes.
-- Map scripts (embedded in .rfmap files) are layered on top and can override hooks.
--
-- Available API:
--   game.broadcast(message)        — Send chat message to all players
--   game.send_message(id, message) — Send chat message to specific player
--   game.end_round(team)           — End the current round
--   game.start_round()             — Start a new round
--
--   world.get_block(x, y, z)       — Get block type at position
--   world.set_block(x, y, z, type) — Set block at position
--   world.is_solid(x, y, z)        — Check if block is solid
--
--   player.get_position(id)        — Get player position {x, y, z}
--   player.get_health(id)          — Get player health
--   player.get_team(id)            — Get player team
--   player.get_all_players()       — Get list of player IDs
--   player.is_alive(id)            — Check if player is alive
--
--   timer.after(name, delay, callback)   — One-shot timer
--   timer.every(name, interval, callback) — Repeating timer
--   timer.cancel(name)                    — Cancel a timer
--
-- Game hooks (define these to handle events):
--   on_game_init()              — Called once when game scripts are loaded
--   on_player_join(playerId)    — Player connected
--   on_player_leave(playerId)   — Player disconnected
--   on_player_death(id, killerId) — Player died
--   on_match_start()            — Match started
--   on_match_end(winningTeam)   — Match ended
--   on_round_start(roundNum)    — Round started
--   on_round_end(winningTeam)   — Round ended
--   on_block_break(id, x,y,z, blockType)  — Block broken
--   on_block_place(id, x,y,z, blockType)  — Block placed
-- =============================================================================

log("[game scripts] BedWars server scripts loaded")

function test_function()
    for i = 1, 5 do
        log("Test function iteration " .. i)
    end
end

function on_game_init()
    log("[game scripts] Server game scripts initialized")
    test_function()
end
