#pragma once

// ENetClient - Client-side connector
// Connects to a server and provides an ENetConnection for use with ClientSession.

#include "enet_connection.hpp"

#include <enet/enet.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace shared::transport {

class ENetClient {
public:
    ENetClient();
    ~ENetClient();

    ENetClient(const ENetClient&) = delete;
    ENetClient& operator=(const ENetClient&) = delete;

    // =========================================================================
    // Connection
    // =========================================================================

    // Connect to a server.
    // Returns true if connection initiated (not necessarily completed).
    // Use is_connected() or wait for onConnect callback to confirm.
    bool connect(const std::string& host, std::uint16_t port,
                 std::uint32_t timeoutMs = config::kConnectionTimeoutMs);

    // Disconnect from server.
    void disconnect();

    // Check if connected.
    bool is_connected() const;

    // =========================================================================
    // Event processing
    // =========================================================================

    // Process network events. Must be called every frame.
    // timeoutMs: 0 for non-blocking, >0 to wait for events.
    void poll(std::uint32_t timeoutMs = config::kPollTimeoutMs);

    // =========================================================================
    // Connection access
    // =========================================================================

    // Get the connection endpoint (for use with ClientSession).
    // Returns nullptr if not connected.
    std::shared_ptr<ENetConnection> connection() { return connection_; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    // Called when connection is established.
    std::function<void()> onConnect;

    // Called when disconnected (either by server or network error).
    std::function<void()> onDisconnect;

private:
    ENetHost* host_{nullptr};
    std::shared_ptr<ENetConnection> connection_;
};

} // namespace shared::transport
