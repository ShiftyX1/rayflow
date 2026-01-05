#include "bedwars_api.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "engine/modules/voxel/shared/block.hpp"

#include <chrono>
#include <random>
#include <sstream>

namespace bedwars::scripting {

namespace {

// Thread-local RNG for script random functions
thread_local std::mt19937 g_rng{std::random_device{}()};

// Server start time for time() function
const auto g_serverStartTime = std::chrono::steady_clock::now();

} // namespace

BedWarsAPI::BedWarsAPI(BedWarsScriptEngine& engine) : engine_(engine) {}

void BedWarsAPI::register_api(sol::state& lua) {
    // Create 'game' namespace
    auto game = lua.create_named_table("game");
    game["broadcast"] = [this](const std::string& msg) { api_broadcast(msg); };
    game["send_message"] = [this](std::uint32_t pid, const std::string& msg) { 
        api_send_message(pid, msg); 
    };
    game["end_round"] = [this](int team) { api_end_round(team); };
    game["start_round"] = [this]() { api_start_round(); };
    
    // Create 'world' namespace
    auto world = lua.create_named_table("world");
    world["get_block"] = [this](int x, int y, int z) { return api_get_block(x, y, z); };
    world["set_block"] = [this](int x, int y, int z, int bt) { api_set_block(x, y, z, bt); };
    world["is_solid"] = [this](int x, int y, int z) { return api_is_solid(x, y, z); };
    
    // Create 'player' namespace
    auto player = lua.create_named_table("player");
    player["get_position"] = [this](sol::this_state ts, std::uint32_t pid) { 
        return api_get_player_position(ts, pid); 
    };
    player["get_health"] = [this](std::uint32_t pid) { return api_get_player_health(pid); };
    player["get_team"] = [this](std::uint32_t pid) { return api_get_player_team(pid); };
    player["get_all"] = [this](sol::this_state ts) { return api_get_all_players(ts); };
    player["is_alive"] = [this](std::uint32_t pid) { return api_is_player_alive(pid); };
    
    // Create 'timer' namespace
    auto timer = lua.create_named_table("timer");
    timer["after"] = [this, &lua](double delay, sol::function callback) {
        api_timer_after(lua, delay, callback);
    };
    timer["every"] = [this, &lua](double interval, sol::function callback) {
        api_timer_every(lua, interval, callback);
    };
    timer["named"] = [this, &lua](const std::string& name, double delay, sol::function callback) {
        api_timer_named(lua, name, delay, callback);
    };
    timer["cancel"] = [this](const std::string& name) { api_timer_cancel(name); };
    
    // Utility functions in global scope
    lua["random"] = [this]() { return api_random(); };
    lua["random_int"] = [this](int min, int max) { return api_random_int(min, max); };
    lua["server_time"] = [this]() { return api_time(); };
    
    // Override print with our log function
    lua["print"] = [this](sol::variadic_args va, sol::this_state ts) { api_log(va, ts); };
    lua["log"] = [this](sol::variadic_args va, sol::this_state ts) { api_log(va, ts); };
}

// Game API implementations

void BedWarsAPI::api_broadcast(const std::string& message) {
    ScriptCommand cmd;
    cmd.type = ScriptCommand::Type::Broadcast;
    cmd.stringParam = message;
    engine_.queue_command(std::move(cmd));
}

void BedWarsAPI::api_send_message(std::uint32_t playerId, const std::string& message) {
    ScriptCommand cmd;
    cmd.type = ScriptCommand::Type::Broadcast;  // TODO: Add SendToPlayer type
    cmd.stringParam = message;
    cmd.playerIdParam = playerId;
    engine_.queue_command(std::move(cmd));
}

void BedWarsAPI::api_end_round(int winningTeam) {
    ScriptCommand cmd;
    cmd.type = ScriptCommand::Type::EndRound;
    cmd.intParams[0] = winningTeam;
    engine_.queue_command(std::move(cmd));
}

void BedWarsAPI::api_start_round() {
    // Start round is typically handled by engine, but scripts can request it
    ScriptCommand cmd;
    cmd.type = ScriptCommand::Type::None;  // TODO: Add StartRound type
    engine_.queue_command(std::move(cmd));
}

// World API implementations

int BedWarsAPI::api_get_block(int x, int y, int z) {
    // TODO: Hook this up to actual terrain query
    // For now, return Air (0)
    (void)x; (void)y; (void)z;
    return static_cast<int>(shared::voxel::BlockType::Air);
}

void BedWarsAPI::api_set_block(int x, int y, int z, int blockType) {
    // Validate block type
    if (blockType < 0 || blockType >= static_cast<int>(shared::voxel::BlockType::Count)) {
        return;
    }
    
    ScriptCommand cmd;
    cmd.type = ScriptCommand::Type::SetBlock;
    cmd.intParams[0] = x;
    cmd.intParams[1] = y;
    cmd.intParams[2] = z;
    cmd.intParams[3] = blockType;
    engine_.queue_command(std::move(cmd));
}

bool BedWarsAPI::api_is_solid(int x, int y, int z) {
    int blockType = api_get_block(x, y, z);
    return shared::voxel::util::is_solid(static_cast<shared::voxel::BlockType>(blockType));
}

// Player API implementations

sol::table BedWarsAPI::api_get_player_position(sol::this_state ts, std::uint32_t playerId) {
    sol::state_view lua(ts);
    auto result = lua.create_table();
    
    // TODO: Hook up to actual player position from server state
    // For now, return dummy position
    (void)playerId;
    result["x"] = 0.0f;
    result["y"] = 0.0f;
    result["z"] = 0.0f;
    
    return result;
}

float BedWarsAPI::api_get_player_health(std::uint32_t playerId) {
    // TODO: Hook up to actual player health
    (void)playerId;
    return 100.0f;
}

int BedWarsAPI::api_get_player_team(std::uint32_t playerId) {
    // TODO: Hook up to actual player team
    (void)playerId;
    return 0;
}

sol::table BedWarsAPI::api_get_all_players(sol::this_state ts) {
    sol::state_view lua(ts);
    auto result = lua.create_table();
    
    // TODO: Hook up to actual player list
    // For now, return empty table
    
    return result;
}

bool BedWarsAPI::api_is_player_alive(std::uint32_t playerId) {
    // TODO: Hook up to actual player alive state
    (void)playerId;
    return true;
}

// Timer implementations

void BedWarsAPI::api_timer_after(sol::state_view lua, double delaySec, sol::function callback) {
    // Store callback as a global function and schedule timer
    std::string funcName = "__timer_cb_" + std::to_string(++anonymousTimerCounter_);
    lua[funcName] = callback;
    engine_.add_timer(funcName, delaySec, 0.0, funcName);
}

void BedWarsAPI::api_timer_every(sol::state_view lua, double intervalSec, sol::function callback) {
    std::string funcName = "__timer_cb_" + std::to_string(++anonymousTimerCounter_);
    lua[funcName] = callback;
    engine_.add_timer(funcName, intervalSec, intervalSec, funcName);
}

void BedWarsAPI::api_timer_named(sol::state_view lua, const std::string& name, double delaySec, sol::function callback) {
    std::string funcName = "__timer_named_" + name;
    lua[funcName] = callback;
    engine_.add_timer(name, delaySec, 0.0, funcName);
}

void BedWarsAPI::api_timer_cancel(const std::string& name) {
    engine_.cancel_timer(name);
}

// Utility implementations

float BedWarsAPI::api_random() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(g_rng);
}

int BedWarsAPI::api_random_int(int min, int max) {
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(g_rng);
}

double BedWarsAPI::api_time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - g_serverStartTime).count();
}

void BedWarsAPI::api_log(sol::variadic_args va, sol::this_state ts) {
    sol::state_view lua(ts);
    std::ostringstream oss;
    
    bool first = true;
    for (auto v : va) {
        if (!first) oss << "\t";
        first = false;
        
        sol::protected_function tostring = lua["tostring"];
        if (tostring.valid()) {
            auto result = tostring(v);
            if (result.valid()) {
                oss << result.get<std::string>();
            }
        }
    }
    
    if (engine_.log_callback()) {
        engine_.log_callback()("[script] " + oss.str());
    }
}

// Block type constants

void BlockTypes::register_constants(sol::state& lua) {
    auto block = lua.create_named_table("BLOCK");
    
    // Core block types (matching shared::voxel::BlockType)
    block["AIR"] = static_cast<int>(shared::voxel::BlockType::Air);
    block["STONE"] = static_cast<int>(shared::voxel::BlockType::Stone);
    block["DIRT"] = static_cast<int>(shared::voxel::BlockType::Dirt);
    block["GRASS"] = static_cast<int>(shared::voxel::BlockType::Grass);
    block["SAND"] = static_cast<int>(shared::voxel::BlockType::Sand);
    block["WATER"] = static_cast<int>(shared::voxel::BlockType::Water);
    block["WOOD"] = static_cast<int>(shared::voxel::BlockType::Wood);
    block["LEAVES"] = static_cast<int>(shared::voxel::BlockType::Leaves);
    block["BEDROCK"] = static_cast<int>(shared::voxel::BlockType::Bedrock);
    block["GRAVEL"] = static_cast<int>(shared::voxel::BlockType::Gravel);
    block["COAL"] = static_cast<int>(shared::voxel::BlockType::Coal);
    block["IRON"] = static_cast<int>(shared::voxel::BlockType::Iron);
    block["GOLD"] = static_cast<int>(shared::voxel::BlockType::Gold);
    block["DIAMOND"] = static_cast<int>(shared::voxel::BlockType::Diamond);
    block["LIGHT"] = static_cast<int>(shared::voxel::BlockType::Light);
    
    // Count (useful for iteration)
    block["COUNT"] = static_cast<int>(shared::voxel::BlockType::Count);
}

// Team constants

void TeamConstants::register_constants(sol::state& lua) {
    auto team = lua.create_named_table("TEAM");
    
    team["NONE"] = 0;
    team["RED"] = 1;
    team["BLUE"] = 2;
    team["GREEN"] = 3;
    team["YELLOW"] = 4;
    
    // Aliases
    team["SPECTATOR"] = 0;
}

} // namespace bedwars::scripting
