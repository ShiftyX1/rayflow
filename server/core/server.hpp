#pragma once

#include "../../shared/transport/endpoint.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "../voxel/terrain.hpp"
#include "../game/game_state.hpp"

namespace server::scripting {
class ScriptEngine;
}

namespace server::core {

class Server {
public:
    struct Options {
        struct LoggingOptions {
            bool enabled{true};
            bool init{true};
            bool rx{true};
            bool tx{true};
            bool move{true};
            bool coll{true};
        } logging{};

        // MT-1: main game prefers latest .rfmap when present; the map editor needs to start empty.
        bool loadLatestMapTemplateFromDisk{true};

        // Map editor: free-fly camera movement (no gravity/collision), driven by InputFrame camUp/camDown.
        bool editorCameraMode{false};
    };

    explicit Server(std::shared_ptr<shared::transport::IEndpoint> endpoint);
    Server(std::shared_ptr<shared::transport::IEndpoint> endpoint, Options opts);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop();

private:
    void run_loop_();
    void tick_once_();
    void handle_message_(shared::proto::Message& msg);
    
    // Script engine integration
    void init_script_engine_();
    void process_script_commands_();

    std::shared_ptr<shared::transport::IEndpoint> endpoint_;

    Options opts_{};

    std::atomic<bool> running_{false};
    std::thread thread_{};

    std::uint32_t tickRate_{30};
    std::uint64_t serverTick_{0};

    std::uint32_t worldSeed_{0};
    std::unique_ptr<server::voxel::Terrain> terrain_{};
    
    // Lua scripting engine for map scripts
    std::unique_ptr<server::scripting::ScriptEngine> scriptEngine_{};

    bool hasMapTemplate_{false};
    std::string mapId_{};
    std::uint32_t mapVersion_{0};

    bool helloSeen_{false};
    bool joined_{false};
    shared::proto::PlayerId playerId_{1};

    // Very temporary: authoritative position placeholder
    float px_{50.0f};
    float py_{80.0f};
    float pz_{50.0f};

    // Authoritative velocity (server-owned)
    float vx_{0.0f};
    float vy_{0.0f};
    float vz_{0.0f};

    bool onGround_{false};
    bool lastJumpHeld_{false};

    shared::proto::InputFrame lastInput_{};

    // Rate-limit noisy input logs (approx. 1Hz). Logging every frame is too spammy at 30 TPS.
    std::uint64_t lastInputLogTick_{0};
    
    // BedWars game state
    std::unique_ptr<server::game::GameState> gameState_;
    
    // Setup game state callbacks
    void setup_game_callbacks_();
    
    // Sync physics position to game state
    void sync_player_position_(float x, float y, float z);
};

} // namespace server::core
