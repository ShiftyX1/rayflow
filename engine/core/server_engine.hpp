#pragma once

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

class ServerEngine : public IEngineServices {
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

// ============================================================================
// Implementation (header-only for simplicity)
// ============================================================================

inline ServerEngine::ServerEngine()
    : ServerEngine(Config{})
{
}

inline ServerEngine::ServerEngine(const Config& config)
    : config_(config)
    , tickDt_(1.0f / config.tickRate)
{
}

inline ServerEngine::~ServerEngine() {
    stop();
}

inline void ServerEngine::set_transport(std::shared_ptr<transport::IServerTransport> transport) {
    transport_ = std::move(transport);
}

inline void ServerEngine::run(IGameServer& game) {
    if (!transport_) {
        log(LogLevel::Error, "No transport set");
        return;
    }
    
    game_ = &game;
    running_ = true;
    
    // Setup transport callbacks
    transport_->onClientConnect = [this, &game](transport::ClientId id) {
        log(LogLevel::Info, "Player connected: " + std::to_string(id));
        game.on_player_connect(static_cast<PlayerId>(id));
    };
    
    transport_->onClientDisconnect = [this, &game](transport::ClientId id) {
        log(LogLevel::Info, "Player disconnected: " + std::to_string(id));
        game.on_player_disconnect(static_cast<PlayerId>(id));
    };
    
    transport_->onReceive = [this, &game](transport::ClientId id, std::span<const std::uint8_t> data) {
        game.on_player_message(static_cast<PlayerId>(id), data);
    };
    
    // Initialize game
    game.on_init(*this);
    log(LogLevel::Info, "Server started at " + std::to_string(config_.tickRate) + " TPS");
    
    // Run tick loop
    tick_loop(game);
    
    // Shutdown
    game.on_shutdown();
    log(LogLevel::Info, "Server stopped");
    game_ = nullptr;
}

inline void ServerEngine::stop() {
    running_ = false;
}

inline void ServerEngine::send(PlayerId id, std::span<const std::uint8_t> data) {
    if (transport_) {
        transport_->send(static_cast<transport::ClientId>(id), data);
    }
}

inline void ServerEngine::broadcast(std::span<const std::uint8_t> data) {
    if (transport_) {
        transport_->broadcast(data);
    }
}

inline void ServerEngine::disconnect(PlayerId id) {
    if (transport_) {
        transport_->disconnect(static_cast<transport::ClientId>(id));
    }
}

inline void ServerEngine::log(LogLevel level, std::string_view msg) {
    if (!config_.logging) return;
    
    const char* prefix = "";
    switch (level) {
        case LogLevel::Debug:   prefix = "[DEBUG] "; break;
        case LogLevel::Info:    prefix = "[INFO]  "; break;
        case LogLevel::Warning: prefix = "[WARN]  "; break;
        case LogLevel::Error:   prefix = "[ERROR] "; break;
    }
    
    // Simple console logging
    std::printf("%s%.*s\n", prefix, static_cast<int>(msg.size()), msg.data());
}

inline void ServerEngine::tick_loop(IGameServer& game) {
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double>;
    
    const Duration tickDuration(tickDt_);
    auto nextTick = Clock::now();
    
    while (running_) {
        auto now = Clock::now();
        
        if (now >= nextTick) {
            // Poll network
            transport_->poll(0);
            
            // Game tick
            game.on_tick(tickDt_);
            ++tick_;
            
            nextTick += std::chrono::duration_cast<Clock::duration>(tickDuration);
            
            // If we're behind, catch up (but don't spiral)
            if (now > nextTick) {
                nextTick = now + std::chrono::duration_cast<Clock::duration>(tickDuration);
            }
        } else {
            // Sleep until next tick
            std::this_thread::sleep_until(nextTick);
        }
    }
}

} // namespace engine
