#pragma once

// ENetServer - Server-side connection manager
// Listens for incoming connections and manages multiple clients.

#include "enet_connection.hpp"

#include <enet/enet.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace shared::transport {

class ENetServer {
public:
    ENetServer();
    ~ENetServer();

    ENetServer(const ENetServer&) = delete;
    ENetServer& operator=(const ENetServer&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    // Start listening on the specified port.
    // Returns true on success, false if bind failed.
    bool start(std::uint16_t port, std::size_t maxClients = config::kDefaultMaxClients);

    // Stop the server and disconnect all clients.
    void stop();

    // Check if server is running.
    bool is_running() const { return running_; }

    // =========================================================================
    // Event processing
    // =========================================================================

    // Process network events. Must be called every tick.
    // timeoutMs: 0 for non-blocking, >0 to wait for events.
    void poll(std::uint32_t timeoutMs = config::kPollTimeoutMs);

    // =========================================================================
    // Client management
    // =========================================================================

    // Get all active connections.
    const std::vector<std::shared_ptr<ENetConnection>>& connections() const {
        return connections_;
    }

    // Find connection by peer pointer.
    std::shared_ptr<ENetConnection> find_connection(ENetPeer* peer);

    // Broadcast a message to all connected clients.
    void broadcast(const shared::proto::Message& msg);

    // =========================================================================
    // Callbacks
    // =========================================================================

    // Called when a new client connects.
    std::function<void(std::shared_ptr<ENetConnection>)> onConnect;

    // Called when a client disconnects.
    std::function<void(std::shared_ptr<ENetConnection>)> onDisconnect;

private:
    void handle_connect(ENetEvent& event);
    void handle_disconnect(ENetEvent& event);
    void handle_receive(ENetEvent& event);

    ENetHost* host_{nullptr};
    std::vector<std::shared_ptr<ENetConnection>> connections_;
    bool running_{false};
};

} // namespace shared::transport
