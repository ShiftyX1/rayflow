#pragma once


#include "../protocol/messages.hpp"

#include <enet/enet.h>

#include <cstdint>
#include <vector>


namespace shared::transport {

class ENetInitializer {
public:
    ENetInitializer() {
        initialized_ = (enet_initialize() == 0);
    }

    ~ENetInitializer() {
        if (initialized_) {
            enet_deinitialize();
        }
    }

    ENetInitializer(const ENetInitializer&) = delete;
    ENetInitializer& operator=(const ENetInitializer&) = delete;

    bool isInitialized() const {
        return initialized_;
    }
    
private:
    bool initialized_ = false;
};

enum class ENetChannel : std::uint8_t {
    Reliable = 0,
    Unreliable = 1,
    ReliableOrdered = 2,

    Count = 3
};

inline ENetChannel get_channel_for_message(const shared::proto::Message& msg) {
    return std::visit([](const auto& m) -> ENetChannel {
        using T = std::decay_t<decltype(m)>;

        if constexpr (std::is_same_v<T, shared::proto::StateSnapshot> ||
                      std::is_same_v<T, shared::proto::InputFrame>) {
            return ENetChannel::Unreliable;
        }
        return ENetChannel::Reliable;
    }, msg);
}

inline enet_uint32 get_packet_flags_for_message(const shared::proto::Message& msg) {
    ENetChannel channel = get_channel_for_message(msg);
    if (channel == ENetChannel::Unreliable) {
        return ENET_PACKET_FLAG_UNSEQUENCED;
    }
    return ENET_PACKET_FLAG_RELIABLE;
}

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