#pragma once

// =============================================================================
// Engine Transport - Raw byte transport interfaces
// Provides IClientTransport and IServerTransport for network abstraction.
// =============================================================================

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace engine::transport {

/// Callback for received data.
using OnReceiveCallback = std::function<void(std::span<const std::uint8_t> data)>;

/// Callback for connection events.
using OnConnectCallback = std::function<void()>;
using OnDisconnectCallback = std::function<void()>;

// --- Client Transport ---

class IClientTransport {
public:
    virtual ~IClientTransport() = default;

    /// Send raw data to the server.
    virtual void send(std::span<const std::uint8_t> data) = 0;
    
    /// Poll for network events. Must be called every frame.
    /// @param timeoutMs 0 for non-blocking.
    virtual void poll(std::uint32_t timeoutMs = 0) = 0;
    
    /// Check if connected.
    virtual bool is_connected() const = 0;
    
    /// Disconnect from server.
    virtual void disconnect() = 0;
    
    /// Get current ping in milliseconds (0 if not available).
    virtual std::uint32_t ping_ms() const { return 0; }

    // Callbacks
    OnReceiveCallback onReceive;
    OnConnectCallback onConnect;
    OnDisconnectCallback onDisconnect;
};

// --- Server Transport ---

using ClientId = std::uint32_t;
static constexpr ClientId kInvalidClientId = 0;

/// Callback for received data from a specific client.
using OnClientReceiveCallback = std::function<void(ClientId id, std::span<const std::uint8_t> data)>;

/// Callback for client connection events.
using OnClientConnectCallback = std::function<void(ClientId id)>;
using OnClientDisconnectCallback = std::function<void(ClientId id)>;

class IServerTransport {
public:
    virtual ~IServerTransport() = default;

    /// Send raw data to a specific client.
    virtual void send(ClientId id, std::span<const std::uint8_t> data) = 0;
    
    /// Broadcast raw data to all connected clients.
    virtual void broadcast(std::span<const std::uint8_t> data) = 0;
    
    /// Poll for network events. Must be called every tick.
    /// @param timeoutMs 0 for non-blocking.
    virtual void poll(std::uint32_t timeoutMs = 0) = 0;
    
    /// Disconnect a specific client.
    virtual void disconnect(ClientId id) = 0;

    // Callbacks
    OnClientReceiveCallback onReceive;
    OnClientConnectCallback onClientConnect;
    OnClientDisconnectCallback onClientDisconnect;
};

} // namespace engine::transport
