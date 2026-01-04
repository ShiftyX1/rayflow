#pragma once

#include "transport.hpp"

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace engine::transport {

// ============================================================================
// LocalTransport - In-process transport for singleplayer
// ============================================================================

/// Creates a linked client/server pair that communicate in-process.
/// No network overhead, perfect for singleplayer or testing.

class LocalClientTransport;
class LocalServerTransport;

struct LocalTransportPair {
    std::shared_ptr<LocalClientTransport> client;
    std::shared_ptr<LocalServerTransport> server;
};

LocalTransportPair create_local_transport_pair();

// ============================================================================
// LocalClientTransport
// ============================================================================

class LocalClientTransport : public IClientTransport {
    friend LocalTransportPair create_local_transport_pair();
    friend class LocalServerTransport;

public:
    void send(std::span<const std::uint8_t> data) override;
    void poll(std::uint32_t timeoutMs = 0) override;
    bool is_connected() const override { return connected_; }
    void disconnect() override;

private:
    LocalClientTransport() = default;
    
    void set_server(std::weak_ptr<LocalServerTransport> server) { server_ = server; }
    void receive(std::vector<std::uint8_t> data);

    std::weak_ptr<LocalServerTransport> server_;
    
    std::mutex mutex_;
    std::queue<std::vector<std::uint8_t>> incoming_;
    bool connected_{false};
    bool connect_pending_{true};
};

// ============================================================================
// LocalServerTransport
// ============================================================================

class LocalServerTransport : public IServerTransport {
    friend LocalTransportPair create_local_transport_pair();
    friend class LocalClientTransport;

public:
    void send(ClientId id, std::span<const std::uint8_t> data) override;
    void broadcast(std::span<const std::uint8_t> data) override;
    void poll(std::uint32_t timeoutMs = 0) override;
    void disconnect(ClientId id) override;

private:
    LocalServerTransport() = default;
    
    void set_client(std::weak_ptr<LocalClientTransport> client) { client_ = client; }
    void receive(std::vector<std::uint8_t> data);

    std::weak_ptr<LocalClientTransport> client_;
    
    std::mutex mutex_;
    std::queue<std::vector<std::uint8_t>> incoming_;
    bool client_connected_{false};
    bool connect_pending_{true};
    
    static constexpr ClientId kLocalClientId = 1;
};

} // namespace engine::transport
