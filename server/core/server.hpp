#pragma once

#include "../../shared/transport/endpoint.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "../voxel/terrain.hpp"

namespace server::core {

class Server {
public:
    explicit Server(std::shared_ptr<shared::transport::IEndpoint> endpoint);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop();

private:
    void run_loop_();
    void tick_once_();
    void handle_message_(shared::proto::Message& msg);

    std::shared_ptr<shared::transport::IEndpoint> endpoint_;

    std::atomic<bool> running_{false};
    std::thread thread_{};

    std::uint32_t tickRate_{30};
    std::uint64_t serverTick_{0};

    std::uint32_t worldSeed_{0};
    std::unique_ptr<server::voxel::Terrain> terrain_{};

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
};

} // namespace server::core
