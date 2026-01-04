#include "enet_server.hpp"

#include <enet/enet.h>
#include <cstdio>

namespace engine::transport {

ENetServerTransport::ENetServerTransport() = default;

ENetServerTransport::~ENetServerTransport() {
    stop();
}

bool ENetServerTransport::start(std::uint16_t port, std::size_t maxClients) {
    if (running_) {
        return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    std::fprintf(stderr, "[enet_server] starting on port %u, maxClients=%zu\n",
                 port, maxClients);

    host_ = enet_host_create(
        &address,
        maxClients,
        config::kChannelCount,
        0,  // Unlimited incoming bandwidth
        0   // Unlimited outgoing bandwidth
    );

    if (!host_) {
        std::fprintf(stderr, "[enet_server] enet_host_create FAILED\n");
        return false;
    }

    enet_host_compress_with_range_coder(host_);

    running_ = true;
    std::fprintf(stderr, "[enet_server] started successfully\n");
    return true;
}

void ENetServerTransport::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Disconnect all clients
    for (auto& [id, peer] : clients_) {
        enet_peer_disconnect(peer, 0);
    }

    // Flush pending packets
    if (host_) {
        for (int i = 0; i < 10; ++i) {
            enet_host_service(host_, nullptr, 10);
        }
        enet_host_destroy(host_);
        host_ = nullptr;
    }

    clients_.clear();
    peerToClient_.clear();
    
    std::fprintf(stderr, "[enet_server] stopped\n");
}

void ENetServerTransport::send(ClientId id, std::span<const std::uint8_t> data) {
    auto it = clients_.find(id);
    if (it == clients_.end()) return;

    ENetPacket* packet = enet_packet_create(
        data.data(),
        data.size(),
        ENET_PACKET_FLAG_RELIABLE
    );

    if (packet) {
        enet_peer_send(it->second, static_cast<std::uint8_t>(Channel::Reliable), packet);
    }
}

void ENetServerTransport::broadcast(std::span<const std::uint8_t> data) {
    if (!host_) return;

    ENetPacket* packet = enet_packet_create(
        data.data(),
        data.size(),
        ENET_PACKET_FLAG_RELIABLE
    );

    if (packet) {
        enet_host_broadcast(host_, static_cast<std::uint8_t>(Channel::Reliable), packet);
    }
}

void ENetServerTransport::poll(std::uint32_t timeoutMs) {
    if (!host_) return;

    ENetEvent event;
    while (enet_host_service(host_, &event, timeoutMs) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                ClientId id = next_client_id();
                clients_[id] = event.peer;
                peerToClient_[event.peer] = id;
                event.peer->data = reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
                
                std::fprintf(stderr, "[enet_server] client %u connected from %x:%u\n",
                             id, event.peer->address.host, event.peer->address.port);
                
                if (onClientConnect) {
                    onClientConnect(id);
                }
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                ClientId id = find_client_id(event.peer);
                if (id != kInvalidClientId) {
                    std::fprintf(stderr, "[enet_server] client %u disconnected\n", id);
                    
                    if (onClientDisconnect) {
                        onClientDisconnect(id);
                    }
                    
                    clients_.erase(id);
                    peerToClient_.erase(event.peer);
                }
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE: {
                ClientId id = find_client_id(event.peer);
                if (id != kInvalidClientId && onReceive) {
                    onReceive(id, std::span<const std::uint8_t>(
                        event.packet->data,
                        event.packet->dataLength
                    ));
                }
                enet_packet_destroy(event.packet);
                break;
            }

            default:
                break;
        }
        
        // Only wait on first iteration
        timeoutMs = 0;
    }
}

void ENetServerTransport::disconnect(ClientId id) {
    auto it = clients_.find(id);
    if (it == clients_.end()) return;

    enet_peer_disconnect(it->second, 0);
    
    if (onClientDisconnect) {
        onClientDisconnect(id);
    }
    
    peerToClient_.erase(it->second);
    clients_.erase(it);
}

ClientId ENetServerTransport::next_client_id() {
    return nextClientId_++;
}

ClientId ENetServerTransport::find_client_id(ENetPeer* peer) {
    auto it = peerToClient_.find(peer);
    return (it != peerToClient_.end()) ? it->second : kInvalidClientId;
}

} // namespace engine::transport
