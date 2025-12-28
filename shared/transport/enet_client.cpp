#include "enet_client.hpp"

#include <cstdio>
#include <cstring>

namespace shared::transport {

ENetClient::ENetClient() = default;

ENetClient::~ENetClient() {
    disconnect();
}

// =============================================================================
// Connection
// =============================================================================

bool ENetClient::connect(const std::string& host, std::uint16_t port,
                         std::uint32_t timeoutMs) {
    // Already connected?
    if (connection_ && connection_->is_connected()) {
        std::fprintf(stderr, "[enet_client] already connected\n");
        return false;
    }

    // Create client host (0 = don't accept incoming connections)
    host_ = enet_host_create(
        nullptr,  // No address - we're a client
        1,        // Only one outgoing connection
        static_cast<std::size_t>(ENetChannel::Count),
        0,        // Unlimited incoming bandwidth
        0         // Unlimited outgoing bandwidth
    );

    if (!host_) {
        std::fprintf(stderr, "[enet_client] enet_host_create failed\n");
        return false;
    }

    // Enable compression to match server
    enet_host_compress_with_range_coder(host_);

    // Resolve host address
    ENetAddress address;
    address.port = port;
    
    // Try enet_address_set_host_ip first (for IP addresses)
    // Fall back to enet_address_set_host (for hostnames with DNS lookup)
    int result = enet_address_set_host_ip(&address, host.c_str());
    if (result < 0) {
        // Try DNS lookup
        result = enet_address_set_host(&address, host.c_str());
    }
    
    if (result < 0) {
        std::fprintf(stderr, "[enet_client] failed to resolve host: %s\n", host.c_str());
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    std::fprintf(stderr, "[enet_client] connecting to %s:%u (timeout=%ums)...\n", 
                 host.c_str(), port, timeoutMs);

    // Initiate connection
    ENetPeer* peer = enet_host_connect(
        host_,
        &address,
        static_cast<std::size_t>(ENetChannel::Count),
        0  // No data
    );

    if (!peer) {
        std::fprintf(stderr, "[enet_client] enet_host_connect failed\n");
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    // Create connection wrapper (in Connecting state)
    connection_ = std::make_shared<ENetConnection>(peer);

    // Wait for connection or timeout
    ENetEvent event;
    int serviceResult = enet_host_service(host_, &event, timeoutMs);
    
    if (serviceResult > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        // Connected!
        std::fprintf(stderr, "[enet_client] connected! ping=%ums\n", peer->roundTripTime);
        connection_->on_connect();
        if (onConnect) {
            onConnect();
        }
        return true;
    }

    // Connection failed or timed out
    if (serviceResult < 0) {
        std::fprintf(stderr, "[enet_client] enet_host_service error\n");
    } else if (serviceResult == 0) {
        std::fprintf(stderr, "[enet_client] connection timed out\n");
    } else {
        std::fprintf(stderr, "[enet_client] unexpected event type: %d\n", event.type);
    }
    
    enet_peer_reset(peer);
    connection_.reset();
    enet_host_destroy(host_);
    host_ = nullptr;
    return false;
}

void ENetClient::disconnect() {
    if (connection_) {
        if (connection_->is_connected()) {
            connection_->disconnect();
            
            // Wait briefly for graceful disconnect
            if (host_) {
                ENetEvent event;
                for (int i = 0; i < 10; ++i) {
                    if (enet_host_service(host_, &event, 10) > 0) {
                        if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                            break;
                        }
                        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                            enet_packet_destroy(event.packet);
                        }
                    }
                }
            }
        }
        connection_->on_disconnect();
        connection_.reset();
    }

    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }
}

bool ENetClient::is_connected() const {
    return connection_ && connection_->is_connected();
}

// =============================================================================
// Event processing
// =============================================================================

void ENetClient::poll(std::uint32_t timeoutMs) {
    if (!host_) {
        return;
    }

    ENetEvent event;

    while (enet_host_service(host_, &event, timeoutMs) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                // Shouldn't happen after initial connect, but handle it
                if (connection_) {
                    connection_->on_connect();
                    if (onConnect) {
                        onConnect();
                    }
                }
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                if (connection_) {
                    connection_->on_disconnect();
                    if (onDisconnect) {
                        onDisconnect();
                    }
                }
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                if (connection_) {
                    connection_->on_receive(event.packet->data, event.packet->dataLength);
                }
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_NONE:
                break;
        }

        // Don't wait after first event
        timeoutMs = 0;
    }
}

} // namespace shared::transport
