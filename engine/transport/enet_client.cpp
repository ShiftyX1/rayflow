#include "enet_client.hpp"

#include <enet/enet.h>
#include <cstdio>

namespace engine::transport {

ENetClientTransport::ENetClientTransport() = default;

ENetClientTransport::~ENetClientTransport() {
    disconnect();
}

bool ENetClientTransport::connect(const std::string& host, std::uint16_t port,
                                   std::uint32_t timeoutMs) {
    if (connected_) {
        std::fprintf(stderr, "[enet_client] already connected\n");
        return false;
    }

    // Create client host
    host_ = enet_host_create(
        nullptr,  // No address - we're a client
        1,        // Only one outgoing connection
        config::kChannelCount,
        0,        // Unlimited incoming bandwidth
        0         // Unlimited outgoing bandwidth
    );

    if (!host_) {
        std::fprintf(stderr, "[enet_client] enet_host_create failed\n");
        return false;
    }

    enet_host_compress_with_range_coder(host_);

    // Resolve host address
    ENetAddress address;
    address.port = port;
    
    int result = enet_address_set_host_ip(&address, host.c_str());
    if (result < 0) {
        result = enet_address_set_host(&address, host.c_str());
    }
    
    if (result < 0) {
        std::fprintf(stderr, "[enet_client] failed to resolve host: %s\n", host.c_str());
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    std::fprintf(stderr, "[enet_client] connecting to %s:%u...\n", host.c_str(), port);

    // Initiate connection
    peer_ = enet_host_connect(host_, &address, config::kChannelCount, 0);
    if (!peer_) {
        std::fprintf(stderr, "[enet_client] enet_host_connect failed\n");
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    // Wait for connection
    ENetEvent event;
    if (enet_host_service(host_, &event, timeoutMs) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        connected_ = true;
        std::fprintf(stderr, "[enet_client] connected! ping=%ums\n", peer_->roundTripTime);
        if (onConnect) {
            onConnect();
        }
        return true;
    }

    std::fprintf(stderr, "[enet_client] connection timed out\n");
    enet_peer_reset(peer_);
    peer_ = nullptr;
    enet_host_destroy(host_);
    host_ = nullptr;
    return false;
}

void ENetClientTransport::send(std::span<const std::uint8_t> data) {
    if (!connected_ || !peer_) return;

    ENetPacket* packet = enet_packet_create(
        data.data(),
        data.size(),
        ENET_PACKET_FLAG_RELIABLE
    );

    if (packet) {
        enet_peer_send(peer_, static_cast<std::uint8_t>(Channel::Reliable), packet);
    }
}

void ENetClientTransport::poll(std::uint32_t timeoutMs) {
    if (!host_) return;

    ENetEvent event;
    while (enet_host_service(host_, &event, timeoutMs) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                if (onReceive) {
                    onReceive(std::span<const std::uint8_t>(
                        event.packet->data,
                        event.packet->dataLength
                    ));
                }
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                connected_ = false;
                peer_ = nullptr;
                if (onDisconnect) {
                    onDisconnect();
                }
                break;

            default:
                break;
        }
        
        // Only wait on first iteration
        timeoutMs = 0;
    }
}

bool ENetClientTransport::is_connected() const {
    return connected_;
}

void ENetClientTransport::disconnect() {
    if (peer_ && connected_) {
        enet_peer_disconnect(peer_, 0);
        
        // Wait for disconnect acknowledgment
        ENetEvent event;
        while (enet_host_service(host_, &event, 100) > 0) {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                break;
            }
            if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                enet_packet_destroy(event.packet);
            }
        }
        
        enet_peer_reset(peer_);
    }

    connected_ = false;
    peer_ = nullptr;

    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }

    if (onDisconnect) {
        onDisconnect();
    }
}

std::uint32_t ENetClientTransport::ping_ms() const {
    if (peer_ && connected_) {
        return peer_->roundTripTime;
    }
    return 0;
}

} // namespace engine::transport
