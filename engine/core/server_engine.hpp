#pragma once

// =============================================================================
// ServerEngine - Fixed tick-rate game server runner
// Runs IGameServer with deterministic tick loop and transport integration.
// =============================================================================

#include "game_interface.hpp"
#include "../transport/transport.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <thread>

namespace engine {

// ============================================================================
// ServerEngine - Runs IGameServer with a fixed tick loop
// ============================================================================

class RAYFLOW_CORE_API ServerEngine : public IEngineServices {
public:
    struct Config {
        float tickRate = 30.0f;
        bool logging = true;
    };

    ServerEngine();
    explicit ServerEngine(const Config& config);
    ~ServerEngine();

    /// Set the transport (must be called before start).
    void set_transport(std::shared_ptr<transport::IServerTransport> transport);

    /// Start the server with the given game.
    /// This runs the tick loop on the current thread (blocking).
    void run(IGameServer& game);

    /// Request shutdown (can be called from another thread).
    void stop();

    // --- IEngineServices implementation ---
    
    void send(PlayerId id, std::span<const std::uint8_t> data) override;
    void broadcast(std::span<const std::uint8_t> data) override;
    void disconnect(PlayerId id) override;
    
    Tick current_tick() const override { return tick_; }
    float tick_rate() const override { return config_.tickRate; }
    float tick_dt() const override { return tickDt_; }
    
    void log(LogLevel level, std::string_view msg) override;

private:
    void tick_loop(IGameServer& game);

    Config config_;
    float tickDt_;
    
    std::shared_ptr<transport::IServerTransport> transport_;
    IGameServer* game_{nullptr};
    
    std::atomic<bool> running_{false};
    Tick tick_{0};
};

} // namespace engine
