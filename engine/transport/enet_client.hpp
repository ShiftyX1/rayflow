#pragma once

#include "transport.hpp"
#include "enet_common.hpp"

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace engine::transport {

// ============================================================================
// ENetClientTransport - Client-side ENet transport
// ============================================================================

class ENetClientTransport : public IClientTransport {
public:
    ENetClientTransport();
    ~ENetClientTransport() override;

    ENetClientTransport(const ENetClientTransport&) = delete;
    ENetClientTransport& operator=(const ENetClientTransport&) = delete;

    // --- Connection ---
    
    /// Connect to a server.
    /// @param host Hostname or IP address.
    /// @param port Port number.
    /// @param timeoutMs Connection timeout in milliseconds.
    /// @return true if connection initiated successfully.
    bool connect(const std::string& host, std::uint16_t port,
                 std::uint32_t timeoutMs = config::kConnectionTimeoutMs);

    // --- IClientTransport implementation ---
    
    void send(std::span<const std::uint8_t> data) override;
    void poll(std::uint32_t timeoutMs = 0) override;
    bool is_connected() const override;
    void disconnect() override;
    std::uint32_t ping_ms() const override;

private:
    ENetHost* host_{nullptr};
    ENetPeer* peer_{nullptr};
    bool connected_{false};
};

} // namespace engine::transport
