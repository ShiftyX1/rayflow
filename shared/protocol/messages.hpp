#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "../voxel/block.hpp"

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
    std::uint32_t worldSeed{0};
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

// Client -> Server: block intents (authoritative server validates/applies)
struct TryPlaceBlock {
    std::uint32_t seq{0};
    int x{0};
    int y{0};
    int z{0};
    shared::voxel::BlockType blockType{shared::voxel::BlockType::Air};
};

struct TryBreakBlock {
    std::uint32_t seq{0};
    int x{0};
    int y{0};
    int z{0};
};

struct StateSnapshot {
    std::uint64_t serverTick{0};
    PlayerId playerId{0};
    float px{0.0f};
    float py{0.0f};
    float pz{0.0f};
    float vx{0.0f};
    float vy{0.0f};
    float vz{0.0f};
};

// Server -> Client: authoritative block results
struct BlockPlaced {
    int x{0};
    int y{0};
    int z{0};
    shared::voxel::BlockType blockType{shared::voxel::BlockType::Air};
};

struct BlockBroken {
    int x{0};
    int y{0};
    int z{0};
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
    TryPlaceBlock,
    TryBreakBlock,
    StateSnapshot,
    BlockPlaced,
    BlockBroken,
    ActionRejected
>;

} // namespace shared::proto
