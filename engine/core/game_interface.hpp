#pragma once

#include "types.hpp"

#include <entt/entt.hpp>

#include <span>
#include <string_view>
#include <cstdint>

// Forward declarations for engine subsystems
namespace voxel {
    class World;
    class BlockInteraction;
}

namespace ui {
    class UIManager;
    struct UIViewModel;
    struct UIFrameInput;
    struct UIFrameOutput;
}

namespace engine {

// ============================================================================
// IEngineServices - Engine provides this to the game server
// ============================================================================

class IEngineServices {
public:
    virtual ~IEngineServices() = default;

    // --- Networking ---
    
    /// Send raw message to a specific player.
    virtual void send(PlayerId id, std::span<const std::uint8_t> data) = 0;
    
    /// Broadcast raw message to all connected players.
    virtual void broadcast(std::span<const std::uint8_t> data) = 0;
    
    /// Disconnect a player.
    virtual void disconnect(PlayerId id) = 0;

    // --- Time ---
    
    /// Current server tick (increments each fixed-timestep update).
    virtual Tick current_tick() const = 0;
    
    /// Server tick rate (ticks per second).
    virtual float tick_rate() const = 0;
    
    /// Fixed delta time per tick (1.0 / tick_rate).
    virtual float tick_dt() const = 0;

    // --- Logging ---
    
    virtual void log(LogLevel level, std::string_view msg) = 0;
    
    void log_debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void log_info(std::string_view msg) { log(LogLevel::Info, msg); }
    void log_warning(std::string_view msg) { log(LogLevel::Warning, msg); }
    void log_error(std::string_view msg) { log(LogLevel::Error, msg); }
};

// ============================================================================
// IGameServer - Game implements this (server-side)
// ============================================================================

class IGameServer {
public:
    virtual ~IGameServer() = default;

    // --- Lifecycle ---
    
    /// Called once when engine starts.
    virtual void on_init(IEngineServices& engine) = 0;
    
    /// Called once when engine shuts down.
    virtual void on_shutdown() = 0;

    // --- Simulation ---
    
    /// Called every server tick with fixed delta time.
    virtual void on_tick(float dt) = 0;

    // --- Players ---
    
    /// Called when a new player connects.
    virtual void on_player_connect(PlayerId id) = 0;
    
    /// Called when a player disconnects.
    virtual void on_player_disconnect(PlayerId id) = 0;
    
    /// Called when a player sends a message.
    /// The game is responsible for deserializing the data.
    virtual void on_player_message(PlayerId id, std::span<const std::uint8_t> data) = 0;
};

// ============================================================================
// IClientServices - Engine provides this to the game client
// ============================================================================

class IClientServices {
public:
    virtual ~IClientServices() = default;

    // --- Networking ---
    
    /// Send raw message to the server.
    virtual void send(std::span<const std::uint8_t> data) = 0;
    
    /// Current connection state.
    virtual ConnectionState connection_state() const = 0;
    
    /// Current ping in milliseconds (0 if not connected or not available).
    virtual std::uint32_t ping_ms() const = 0;

    // --- Time ---
    
    /// Frame delta time (variable).
    virtual float frame_dt() const = 0;

    // --- Window ---
    
    /// Window width in pixels.
    virtual int window_width() const = 0;
    
    /// Window height in pixels.
    virtual int window_height() const = 0;

    // --- Voxel World ---
    
    /// Get the voxel world (engine owns it).
    virtual voxel::World& world() = 0;
    virtual const voxel::World& world() const = 0;
    
    /// Initialize/reset the world with a seed.
    virtual void init_world(std::uint32_t seed) = 0;
    
    /// Get block interaction system.
    virtual voxel::BlockInteraction& block_interaction() = 0;

    // --- ECS ---
    
    /// Get the ECS registry.
    virtual entt::registry& registry() = 0;

    // --- UI ---
    
    /// Get the UI manager.
    virtual ui::UIManager& ui_manager() = 0;

    // --- Logging ---
    
    virtual void log(LogLevel level, std::string_view msg) = 0;
    
    void log_debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void log_info(std::string_view msg) { log(LogLevel::Info, msg); }
    void log_warning(std::string_view msg) { log(LogLevel::Warning, msg); }
    void log_error(std::string_view msg) { log(LogLevel::Error, msg); }
};

// ============================================================================
// IGameClient - Game implements this (client-side)
// ============================================================================

class IGameClient {
public:
    virtual ~IGameClient() = default;

    // --- Lifecycle ---
    
    /// Called once when engine starts.
    virtual void on_init(IClientServices& engine) = 0;
    
    /// Called once when engine shuts down.
    virtual void on_shutdown() = 0;

    // --- Frame Loop ---
    
    /// Called every frame for logic update (variable dt).
    virtual void on_update(float dt) = 0;
    
    /// Called every frame for rendering.
    virtual void on_render() = 0;

    // --- Networking ---
    
    /// Called when connected to server.
    virtual void on_connected() = 0;
    
    /// Called when disconnected from server.
    virtual void on_disconnected() = 0;
    
    /// Called when a message is received from server.
    /// The game is responsible for deserializing the data.
    virtual void on_server_message(std::span<const std::uint8_t> data) = 0;
};

} // namespace engine
