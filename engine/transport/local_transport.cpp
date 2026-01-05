#include "local_transport.hpp"

namespace engine::transport {

// ============================================================================
// Factory
// ============================================================================

LocalTransportPair create_local_transport_pair() {
    auto client = std::shared_ptr<LocalClientTransport>(new LocalClientTransport());
    auto server = std::shared_ptr<LocalServerTransport>(new LocalServerTransport());
    
    client->set_server(server);
    server->set_client(client);
    
    return {client, server};
}

// ============================================================================
// LocalClientTransport
// ============================================================================

void LocalClientTransport::send(std::span<const std::uint8_t> data) {
    // Allow sending if connected OR if connection is pending (will be connected on next poll)
    if (!connected_ && !connect_pending_) return;
    
    if (auto srv = server_.lock()) {
        srv->receive(std::vector<std::uint8_t>(data.begin(), data.end()));
    }
}

void LocalClientTransport::poll(std::uint32_t /*timeoutMs*/) {
    // Handle pending connection
    if (connect_pending_) {
        connect_pending_ = false;
        connected_ = true;
        if (onConnect) {
            onConnect();
        }
    }
    
    // Process incoming messages
    std::queue<std::vector<std::uint8_t>> messages;
    {
        std::lock_guard lock(mutex_);
        std::swap(messages, incoming_);
    }
    
    while (!messages.empty()) {
        auto& msg = messages.front();
        if (onReceive) {
            onReceive(msg);
        }
        messages.pop();
    }
}

void LocalClientTransport::disconnect() {
    if (!connected_) return;
    connected_ = false;
    
    if (auto srv = server_.lock()) {
        srv->client_connected_ = false;
        // Server will notice on next poll
    }
    
    if (onDisconnect) {
        onDisconnect();
    }
}

void LocalClientTransport::receive(std::vector<std::uint8_t> data) {
    std::lock_guard lock(mutex_);
    incoming_.push(std::move(data));
}

// ============================================================================
// LocalServerTransport
// ============================================================================

void LocalServerTransport::send(ClientId id, std::span<const std::uint8_t> data) {
    if (id != kLocalClientId || !client_connected_) return;
    
    if (auto cli = client_.lock()) {
        cli->receive(std::vector<std::uint8_t>(data.begin(), data.end()));
    }
}

void LocalServerTransport::broadcast(std::span<const std::uint8_t> data) {
    send(kLocalClientId, data);
}

void LocalServerTransport::poll(std::uint32_t /*timeoutMs*/) {
    // Handle pending connection
    if (connect_pending_) {
        connect_pending_ = false;
        client_connected_ = true;
        if (onClientConnect) {
            onClientConnect(kLocalClientId);
        }
    }
    
    // Process incoming messages
    std::queue<std::vector<std::uint8_t>> messages;
    {
        std::lock_guard lock(mutex_);
        std::swap(messages, incoming_);
    }
    
    while (!messages.empty()) {
        auto& msg = messages.front();
        if (onReceive) {
            onReceive(kLocalClientId, msg);
        }
        messages.pop();
    }
}

void LocalServerTransport::disconnect(ClientId id) {
    if (id != kLocalClientId || !client_connected_) return;
    client_connected_ = false;
    
    if (auto cli = client_.lock()) {
        cli->connected_ = false;
        if (cli->onDisconnect) {
            cli->onDisconnect();
        }
    }
    
    if (onClientDisconnect) {
        onClientDisconnect(kLocalClientId);
    }
}

void LocalServerTransport::receive(std::vector<std::uint8_t> data) {
    std::lock_guard lock(mutex_);
    incoming_.push(std::move(data));
}

} // namespace engine::transport
