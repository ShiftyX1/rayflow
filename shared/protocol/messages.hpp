#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace shared::proto {

using ProtocolVersion = std::uint32_t;
static constexpr ProtocolVersion kProtocolVersion = 1;

using PlayerId = std::uint32_t;

enum class RejectReason : std::uint8_t {
    Unknown = 0,
    Invalid = 1,
    NotAllowed = 2,
    NotEnoughResources = 3,
    OutOfRange = 4,
    ProtectedBlock = 5,
};

struct ClientHello {
    ProtocolVersion version{kProtocolVersion};
    std::string clientName;
};

struct ServerHello {
    ProtocolVersion acceptedVersion{kProtocolVersion};
    std::uint32_t tickRate{30};
};

struct JoinMatch {
};

struct JoinAck {
    PlayerId playerId{0};
};

struct InputFrame {
    std::uint32_t seq{0};
    float moveX{0.0f};
    float moveY{0.0f};
    float yaw{0.0f};
    float pitch{0.0f};
    bool jump{false};
    bool sprint{false};
};

struct StateSnapshot {
    std::uint64_t serverTick{0};
    PlayerId playerId{0};
    float px{0.0f};
    float py{0.0f};
    float pz{0.0f};
};

struct ActionRejected {
    std::uint32_t seq{0};
    RejectReason reason{RejectReason::Unknown};
};

using Message = std::variant<
    ClientHello,
    ServerHello,
    JoinMatch,
    JoinAck,
    InputFrame,
    StateSnapshot,
    ActionRejected
>;

} // namespace shared::proto
