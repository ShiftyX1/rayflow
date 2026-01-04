#pragma once

#include "bedwars_script_engine.hpp"

#include <sol/forward.hpp>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace bedwars::scripting {

// Game API exposed to map scripts (sandboxed, limited functionality)
// This is the "User" level API for custom game modes
class BedWarsAPI {
public:
    explicit BedWarsAPI(BedWarsScriptEngine& engine);
    
    // Register all API functions with Lua state
    void register_api(sol::state& lua);

private:
    // Game namespace
    void api_broadcast(const std::string& message);
    void api_send_message(std::uint32_t playerId, const std::string& message);
    void api_end_round(int winningTeam);
    void api_start_round();
    
    // World namespace  
    int api_get_block(int x, int y, int z);
    void api_set_block(int x, int y, int z, int blockType);
    bool api_is_solid(int x, int y, int z);
    
    // Player namespace (read-only for user scripts)
    sol::table api_get_player_position(sol::this_state ts, std::uint32_t playerId);
    float api_get_player_health(std::uint32_t playerId);
    int api_get_player_team(std::uint32_t playerId);
    sol::table api_get_all_players(sol::this_state ts);
    bool api_is_player_alive(std::uint32_t playerId);
    
    // Timer namespace
    void api_timer_after(sol::state_view lua, double delaySec, sol::function callback);
    void api_timer_every(sol::state_view lua, double intervalSec, sol::function callback);
    void api_timer_named(sol::state_view lua, const std::string& name, double delaySec, sol::function callback);
    void api_timer_cancel(const std::string& name);
    
    // Utility functions
    float api_random();
    int api_random_int(int min, int max);
    double api_time();  // Server time in seconds
    
    // Log function (replaces print in sandbox)
    void api_log(sol::variadic_args va, sol::this_state ts);
    
    BedWarsScriptEngine& engine_;
    int anonymousTimerCounter_{0};
};

// Block type constants exposed to Lua
struct BlockTypes {
    static void register_constants(sol::state& lua);
};

// Team constants exposed to Lua
struct TeamConstants {
    static void register_constants(sol::state& lua);
};

} // namespace bedwars::scripting
