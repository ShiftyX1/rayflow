#include "enet_server.hpp"

#include <algorithm>
#include <cstdio>

namespace shared::transport {

ENetServer::ENetServer() = default;

ENetServer::~ENetServer() {
    stop();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool ENetServer::start(std::uint16_t port, std::size_t maxClients) {
    if (running_) {
        return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    std::fprintf(stderr, "[enet_server] creating host on port %u, maxClients=%zu\n",
                 port, maxClients);

    host_ = enet_host_create(
        &address,
        maxClients,
        static_cast<std::size_t>(ENetChannel::Count),
        0,  // Unlimited incoming bandwidth
        0   // Unlimited outgoing bandwidth
    );

    if (!host_) {
        std::fprintf(stderr, "[enet_server] enet_host_create FAILED\n");
        return false;
    }

    std::fprintf(stderr, "[enet_server] host created successfully\n");

    // Enable compression for smaller packets
    enet_host_compress_with_range_coder(host_);

    running_ = true;
    return true;
}

void ENetServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Disconnect all clients gracefully
    for (auto& conn : connections_) {
        if (conn->is_connected()) {
            conn->disconnect();
        }
    }

    // Give clients a moment to receive disconnect
    if (host_) {
        // Flush pending packets and process disconnect acknowledgments
        for (int i = 0; i < 10; ++i) {
            enet_host_service(host_, nullptr, 10);
        }

        enet_host_destroy(host_);
        host_ = nullptr;
    }

    connections_.clear();
}

// =============================================================================
// Event processing
// =============================================================================

void ENetServer::poll(std::uint32_t timeoutMs) {
    if (!host_) {
        return;
    }

    ENetEvent event;
    
    // Process all pending events
    while (enet_host_service(host_, &event, timeoutMs) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                handle_connect(event);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                handle_disconnect(event);
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                handle_receive(event);
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
        }

        // Don't wait after first event - process all immediately
        timeoutMs = 0;
    }
}

// =============================================================================
// Client management
// =============================================================================

std::shared_ptr<ENetConnection> ENetServer::find_connection(ENetPeer* peer) {
    for (auto& conn : connections_) {
        if (conn->peer() == peer) {
            return conn;
        }
    }
    return nullptr;
}

void ENetServer::broadcast(const shared::proto::Message& msg) {
    // Serialize once, send to all
    std::vector<std::uint8_t> data = serialize_message(msg);
    ENetChannel channel = get_channel_for_message(msg);
    enet_uint32 flags = get_packet_flags_for_message(msg);

    ENetPacket* packet = enet_packet_create(
        data.data(),
        data.size(),
        flags
    );

    if (!packet) {
        return;
    }

    enet_host_broadcast(host_, static_cast<enet_uint8>(channel), packet);
}

// =============================================================================
// Event handlers
// =============================================================================

void ENetServer::handle_connect(ENetEvent& event) {
    std::fprintf(stderr, "[enet_server] CONNECT from %x:%u\n",
                 event.peer->address.host, event.peer->address.port);

    // Create connection wrapper
    auto conn = std::make_shared<ENetConnection>(event.peer);
    conn->on_connect();

    // Store peer -> connection mapping in peer data
    event.peer->data = conn.get();

    connections_.push_back(conn);

    // Notify callback
    if (onConnect) {
        onConnect(conn);
    }
}

void ENetServer::handle_disconnect(ENetEvent& event) {
    // Find connection
    auto conn = find_connection(event.peer);
    if (!conn) {
        return;
    }

    // Mark as disconnected
    conn->on_disconnect();

    // Notify callback before removing
    if (onDisconnect) {
        onDisconnect(conn);
    }

    // Remove from list
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end()
    );
}

void ENetServer::handle_receive(ENetEvent& event) {
    auto conn = find_connection(event.peer);
    if (!conn) {
        return;
    }

    // Pass to connection for deserialization and queuing
    conn->on_receive(event.packet->data, event.packet->dataLength);
}

} // namespace shared::transport
