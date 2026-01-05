#pragma once

// ENet common utilities for engine transport layer.
// Forward declarations avoid Windows header conflicts.

#include <cstddef>
#include <cstdint>

// Forward declarations for ENet types (opaque pointers)
struct _ENetHost;
struct _ENetPeer;
struct _ENetPacket;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetPacket ENetPacket;

namespace engine::transport {

// ============================================================================
// ENetInitializer - RAII wrapper for enet_initialize/enet_deinitialize
// ============================================================================

class ENetInitializer {
public:
    ENetInitializer();
    ~ENetInitializer();

    ENetInitializer(const ENetInitializer&) = delete;
    ENetInitializer& operator=(const ENetInitializer&) = delete;

    bool is_initialized() const { return initialized_; }
    
private:
    bool initialized_{false};
};

// ============================================================================
// Configuration
// ============================================================================

namespace config {
    constexpr std::uint16_t kDefaultPort = 7777;
    constexpr std::size_t kDefaultMaxClients = 32;
    constexpr std::uint32_t kConnectionTimeoutMs = 5000;
    constexpr std::uint32_t kPollTimeoutMs = 0;
    constexpr std::size_t kChannelCount = 2;  // Reliable + Unreliable
}

// ============================================================================
// Channels
// ============================================================================

enum class Channel : std::uint8_t {
    Reliable = 0,
    Unreliable = 1,
};

} // namespace engine::transport
