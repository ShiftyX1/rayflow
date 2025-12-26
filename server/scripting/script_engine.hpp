#pragma once

#include "../../shared/scripting/lua_state.hpp"
#include "../../shared/scripting/sandbox.hpp"
#include "../../shared/scripting/script_utils.hpp"

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace server::scripting {

// Forward declarations
class GameAPI;

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

// Timer entry for script timers
struct ScriptTimer {
    std::string name;
    double remainingSec{0.0};
    double intervalSec{0.0};  // 0 = one-shot, >0 = repeating
    std::string callbackFunc;
    bool cancelled{false};
};

// Server-side script engine for map scripts
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();
    
    // Non-copyable
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;
    
    // Initialize the engine
    bool init();
    
    // Load scripts from MapScriptData
    shared::scripting::ScriptResult load_map_scripts(const shared::scripting::MapScriptData& scripts);
    
    // Unload current scripts
    void unload();
    
    // Check if scripts are loaded
    bool has_scripts() const { return scriptsLoaded_; }
    
    // Update timers and process pending callbacks
    void update(float deltaTime);
    
    // Get and clear pending commands
    std::vector<ScriptCommand> take_commands();
    
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
    
    // Access to underlying Lua state (for advanced usage)
    shared::scripting::LuaState* lua_state() { return lua_.get(); }
    
    // Get last error message
    const std::string& last_error() const { return lastError_; }
    
    // Set logging callback for script print() calls
    void set_log_callback(std::function<void(const std::string&)> callback);

private:
    void setup_game_api();
    void call_hook(const char* hookName);
    
    // Queue a command from script
    void queue_command(ScriptCommand cmd);
    
    // Timer management (called from Lua)
    void add_timer(const std::string& name, double delaySec, double intervalSec, const std::string& callback);
    void cancel_timer(const std::string& name);
    
    std::unique_ptr<shared::scripting::LuaState> lua_;
    std::unique_ptr<GameAPI> api_;
    
    bool scriptsLoaded_{false};
    std::string lastError_;
    
    std::vector<ScriptCommand> pendingCommands_;
    std::vector<ScriptTimer> timers_;
    
    std::function<void(const std::string&)> logCallback_;
    
    friend class GameAPI;
};

} // namespace server::scripting

