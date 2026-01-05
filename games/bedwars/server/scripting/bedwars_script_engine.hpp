#pragma once

#include <engine/core/scripting/script_engine_base.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace bedwars::scripting {

// Command from script to be executed by server
struct ScriptCommand {
    enum class Type : std::uint8_t {
        None = 0,
        Broadcast,           // Send message to all players
        SetBlock,            // Set a block at position
        SpawnEntity,         // Spawn an entity
        TeleportPlayer,      // Teleport a player
        SetPlayerHealth,     // Set player health
        EndRound,            // End the current round
        StartTimer,          // Start a named timer
        CancelTimer,         // Cancel a named timer
    };
    
    Type type{Type::None};
    
    // Generic parameters (interpretation depends on type)
    std::string stringParam;
    int intParams[4]{0, 0, 0, 0};
    float floatParams[4]{0.0f, 0.0f, 0.0f, 0.0f};
    std::uint32_t playerIdParam{0};
};

// Forward declaration
class BedWarsAPI;

// BedWars-specific script engine
// Inherits from engine's base and adds BedWars game API
class BedWarsScriptEngine : public engine::scripting::ScriptEngineBase {
public:
    BedWarsScriptEngine();
    ~BedWarsScriptEngine() override;
    
    // Initialize with default BedWars configuration
    bool init();
    
    // Get and clear pending commands
    std::vector<ScriptCommand> take_commands();
    
    // Queue a command from API
    void queue_command(ScriptCommand cmd);
    
    // Event triggers (call Lua hooks)
    void on_player_join(std::uint32_t playerId);
    void on_player_leave(std::uint32_t playerId);
    void on_player_spawn(std::uint32_t playerId, float x, float y, float z);
    void on_player_death(std::uint32_t playerId, std::uint32_t killerId);
    
    void on_block_break(std::uint32_t playerId, int x, int y, int z, int blockType);
    void on_block_place(std::uint32_t playerId, int x, int y, int z, int blockType);
    
    void on_round_start(int roundNumber);
    void on_round_end(int winningTeam);
    void on_match_start();
    void on_match_end(int winningTeam);
    
    // Custom event (for extensibility)
    void on_custom_event(const std::string& eventName, const std::string& data);
    
    // Timer access for API
    using ScriptEngineBase::add_timer;
    using ScriptEngineBase::cancel_timer;

protected:
    // Register BedWars-specific API (game.*, world.*, player.*, etc.)
    void register_game_api(engine::scripting::LuaState& lua) override;
    
    // Register constants (BLOCK.*, TEAM.*)
    void register_constants(engine::scripting::LuaState& lua) override;

private:
    std::unique_ptr<BedWarsAPI> api_;
    std::vector<ScriptCommand> pendingCommands_;
    
    friend class BedWarsAPI;
};

} // namespace bedwars::scripting
