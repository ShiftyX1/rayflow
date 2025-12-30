#pragma once

// DedicatedServer - Multi-client network server for RFDS
// Manages multiple ENetConnections and per-client game state.

#include "../../shared/transport/enet_server.hpp"
#include "../../shared/transport/enet_connection.hpp"
#include "../../shared/protocol/messages.hpp"
#include "../voxel/terrain.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace server::scripting {
class ScriptEngine;
}

namespace server::core {

struct ClientState {
    enum class Phase {
        Connected,
        Handshaking,
        InGame,
        Disconnecting
    };

    std::shared_ptr<shared::transport::ENetConnection> connection;
    shared::proto::PlayerId playerId{0};
    Phase phase{Phase::Connected};

    float px{50.0f};
    float py{80.0f};
    float pz{50.0f};

    float vx{0.0f};
    float vy{0.0f};
    float vz{0.0f};

    bool onGround{false};
    bool lastJumpHeld{false};

    shared::proto::InputFrame lastInput{};
    std::uint64_t lastInputLogTick{0};
};

class DedicatedServer {
public:
    struct Config {
        std::uint16_t port{7777};
        std::size_t maxClients{16};
        std::uint32_t tickRate{30};
        
        struct Logging {
            bool enabled{true};
            bool init{true};
            bool rx{true};
            bool tx{true};
            bool move{false};
            bool coll{false};
        } logging{};
    };

    explicit DedicatedServer(const Config& config);
    ~DedicatedServer();

    DedicatedServer(const DedicatedServer&) = delete;
    DedicatedServer& operator=(const DedicatedServer&) = delete;

    bool start();

    void stop();

    bool is_running() const { return running_.load(); }

    std::uint64_t server_tick() const { return serverTick_; }

    std::size_t client_count() const;

    std::function<void(shared::proto::PlayerId)> onPlayerJoin;
    std::function<void(shared::proto::PlayerId)> onPlayerLeave;

private:
    void run_loop_();
    void tick_once_();

    void handle_client_connect_(std::shared_ptr<shared::transport::ENetConnection> conn);
    void handle_client_disconnect_(std::shared_ptr<shared::transport::ENetConnection> conn);
    void process_client_messages_(ClientState& client);
    void handle_message_(ClientState& client, shared::proto::Message& msg);

    void simulate_client_(ClientState& client, float dt);

    void broadcast_(const shared::proto::Message& msg);
    void broadcast_except_(const shared::proto::Message& msg, shared::proto::PlayerId except);

    void init_script_engine_();
    void process_script_commands_();

    Config config_;
    shared::transport::ENetServer netServer_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    std::uint32_t tickRate_{30};
    std::uint64_t serverTick_{0};
    shared::proto::PlayerId nextPlayerId_{1};

    std::uint32_t worldSeed_{0};
    std::unique_ptr<server::voxel::Terrain> terrain_;
    std::unique_ptr<server::scripting::ScriptEngine> scriptEngine_;

    bool hasMapTemplate_{false};
    std::string mapId_;
    std::uint32_t mapVersion_{0};

    std::unordered_map<shared::proto::PlayerId, ClientState> clients_;
    
    std::unordered_map<shared::transport::ENetConnection*, shared::proto::PlayerId> connToPlayer_;
};

} // namespace server::core
