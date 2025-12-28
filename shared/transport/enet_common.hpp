#pragma once

// ENet common utilities and types.
// NOTE: This header avoids including <enet/enet.h> directly to prevent
// Windows header conflicts (winsock2.h/windows.h vs raylib CloseWindow/ShowCursor).
// ENet types are forward-declared or wrapped as needed.

#include "../protocol/messages.hpp"

#include <cstdint>
#include <vector>

// Forward declarations for ENet types (opaque pointers)
struct _ENetHost;
struct _ENetPeer;
struct _ENetPacket;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetPacket ENetPacket;
typedef unsigned int enet_uint32;

namespace shared::transport {

// ENetInitializer must be created in a .cpp file that includes <enet/enet.h>
// This is just a forward declaration of its existence.
class ENetInitializer {
public:
    ENetInitializer();
    ~ENetInitializer();

    ENetInitializer(const ENetInitializer&) = delete;
    ENetInitializer& operator=(const ENetInitializer&) = delete;

    bool isInitialized() const { return initialized_; }
    
private:
    bool initialized_ = false;
};

enum class ENetChannel : std::uint8_t {
    Reliable = 0,
    Unreliable = 1,
    ReliableOrdered = 2,

    Count = 3
};

// These functions are implemented in enet_common.cpp
ENetChannel get_channel_for_message(const shared::proto::Message& msg);
enet_uint32 get_packet_flags_for_message(const shared::proto::Message& msg);

std::vector<std::uint8_t> serialize_message(const shared::proto::Message& msg);
bool deserialize_message(const std::uint8_t* data, std::size_t size,
                         shared::proto::Message& outMsg);


namespace config {
    constexpr std::uint16_t kDefaultPort = 7777;
    constexpr std::size_t kDefaultMaxClients = 16;
    constexpr std::uint32_t kConnectionTimeoutMs = 5000;
    constexpr std::uint32_t kPollTimeoutMs = 0;
}
}