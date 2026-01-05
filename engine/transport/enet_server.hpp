#pragma once

#include "transport.hpp"
#include "enet_common.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace engine::transport {

// ============================================================================
// ENetServerTransport - Server-side ENet transport
// ============================================================================

class ENetServerTransport : public IServerTransport {
public:
    ENetServerTransport();
    ~ENetServerTransport() override;

    ENetServerTransport(const ENetServerTransport&) = delete;
    ENetServerTransport& operator=(const ENetServerTransport&) = delete;

    // --- Server control ---
    
    /// Start listening on the specified port.
    /// @param port Port to listen on.
    /// @param maxClients Maximum number of concurrent clients.
    /// @return true if server started successfully.
    bool start(std::uint16_t port = config::kDefaultPort,
               std::size_t maxClients = config::kDefaultMaxClients);

    /// Stop the server and disconnect all clients.
    void stop();

    /// Check if server is running.
    bool is_running() const { return running_; }

    // --- IServerTransport implementation ---
    
    void send(ClientId id, std::span<const std::uint8_t> data) override;
    void broadcast(std::span<const std::uint8_t> data) override;
    void poll(std::uint32_t timeoutMs = 0) override;
    void disconnect(ClientId id) override;

private:
    ClientId next_client_id();
    ClientId find_client_id(ENetPeer* peer);

    ENetHost* host_{nullptr};
    bool running_{false};
    
    ClientId nextClientId_{1};
    std::unordered_map<ClientId, ENetPeer*> clients_;
    std::unordered_map<ENetPeer*, ClientId> peerToClient_;
};

} // namespace engine::transport
