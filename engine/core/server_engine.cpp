#include "server_engine.hpp"
#include "logging.hpp"

#include <cstdio>
#include <string>
#include <thread>

namespace engine {

ServerEngine::ServerEngine()
    : ServerEngine(Config{})
{
}

ServerEngine::ServerEngine(const Config& config)
    : config_(config)
    , tickDt_(1.0f / config.tickRate)
{
}

ServerEngine::~ServerEngine() {
    stop();
}

void ServerEngine::set_transport(std::shared_ptr<transport::IServerTransport> transport) {
    transport_ = std::move(transport);
}

void ServerEngine::run(IGameServer& game) {
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

void ServerEngine::stop() {
    running_ = false;
}

void ServerEngine::send(PlayerId id, std::span<const std::uint8_t> data) {
    if (transport_) {
        transport_->send(static_cast<transport::ClientId>(id), data);
    }
}

void ServerEngine::broadcast(std::span<const std::uint8_t> data) {
    if (transport_) {
        transport_->broadcast(data);
    }
}

void ServerEngine::disconnect(PlayerId id) {
    if (transport_) {
        transport_->disconnect(static_cast<transport::ClientId>(id));
    }
}

void ServerEngine::log(LogLevel level, std::string_view msg) {
    if (!config_.logging) return;
    
    int traceLevel = LOG_INFO;
    switch (level) {
        case LogLevel::Debug:   traceLevel = LOG_DEBUG;   break;
        case LogLevel::Info:    traceLevel = LOG_INFO;    break;
        case LogLevel::Warning: traceLevel = LOG_WARNING; break;
        case LogLevel::Error:   traceLevel = LOG_ERROR;   break;
    }
    
    TraceLog(traceLevel, "%.*s", static_cast<int>(msg.size()), msg.data());
}

void ServerEngine::tick_loop(IGameServer& game) {
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
