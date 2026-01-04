#pragma once

// BedWars Protocol Messages
// Game-specific messages that use engine's ByteWriter/ByteReader for serialization.

#include <engine/core/byte_buffer.hpp>
#include <engine/core/types.hpp>

// BedWars game types
#include "../game/item_types.hpp"
#include "../game/team_types.hpp"
#include <engine/modules/voxel/shared/block.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace bedwars::proto {

// ============================================================================
// Protocol Version
// ============================================================================

using ProtocolVersion = std::uint32_t;
static constexpr ProtocolVersion kProtocolVersion = 1;  // Must match shared::proto for client compatibility

// ============================================================================
// Re-export shared types for convenience
// ============================================================================

using TeamId = bedwars::game::TeamId;
using ItemType = bedwars::game::ItemType;
using BlockType = shared::voxel::BlockType;

namespace Teams = bedwars::game::Teams;

// ============================================================================
// Message Types (for serialization)
// ============================================================================

// MessageType values MUST match shared::proto MessageTypeIndex for client compatibility
// See shared/transport/enet_common.cpp for the canonical list
enum class MessageType : std::uint8_t {
    // Handshake (0-3)
    ClientHello = 0,
    ServerHello = 1,
    JoinMatch = 2,
    JoinAck = 3,
    
    // Input (4)
    InputFrame = 4,
    
    // Blocks (5-7, 9-11)
    TryPlaceBlock = 5,
    TryBreakBlock = 6,
    TrySetBlock = 7,
    // 8 = StateSnapshot
    BlockPlaced = 9,
    BlockBroken = 10,
    ActionRejected = 11,
    
    // State (8, 14)
    StateSnapshot = 8,
    ChunkData = 14,
    
    // Map export (12-13)
    TryExportMap = 12,
    ExportResult = 13,
    
    // Game events (15-21) - these are new, must match shared/transport/enet_common.cpp
    TeamAssigned = 15,
    HealthUpdate = 16,
    PlayerDied = 17,
    PlayerRespawned = 18,
    BedDestroyed = 19,
    TeamEliminated = 20,
    MatchEnded = 21,
    
    // Items (22-24)
    ItemSpawned = 22,
    ItemPickedUp = 23,
    InventoryUpdate = 24,
};

// ============================================================================
// Reject Reasons
// ============================================================================

enum class RejectReason : std::uint8_t {
    Unknown = 0,
    Invalid = 1,
    NotAllowed = 2,
    NotEnoughResources = 3,
    OutOfRange = 4,
    ProtectedBlock = 5,
    Collision = 6,
    NoLineOfSight = 7,
};

// ============================================================================
// Messages - Handshake
// ============================================================================

struct ClientHello {
    ProtocolVersion version{kProtocolVersion};
    std::string clientName;
};

struct ServerHello {
    ProtocolVersion acceptedVersion{kProtocolVersion};
    std::uint32_t tickRate{30};
    std::uint32_t worldSeed{0};
    
    // Map template info (MT-1)
    bool hasMapTemplate{false};
    std::string mapId;
    std::uint32_t mapVersion{0};
};

struct JoinMatch {};

struct JoinAck {
    engine::PlayerId playerId{0};
};

// ============================================================================
// Messages - Input
// ============================================================================

struct InputFrame {
    std::uint32_t seq{0};
    float moveX{0.0f};
    float moveY{0.0f};
    float yaw{0.0f};
    float pitch{0.0f};
    bool jump{false};
    bool sprint{false};
    
    // Editor camera mode
    bool camUp{false};
    bool camDown{false};
};

// ============================================================================
// Messages - State
// ============================================================================

struct StateSnapshot {
    engine::Tick serverTick{0};
    engine::PlayerId playerId{0};
    float px{0.0f};
    float py{0.0f};
    float pz{0.0f};
    float vx{0.0f};
    float vy{0.0f};
    float vz{0.0f};
};

struct ChunkData {
    std::int32_t chunkX{0};
    std::int32_t chunkZ{0};
    // Flat array of blocks: [y][z][x] order, 16x256x16 = 65536 blocks
    std::vector<std::uint8_t> blocks;
};

// ============================================================================
// Messages - Blocks
// ============================================================================

struct TryPlaceBlock {
    std::uint32_t seq{0};
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
    BlockType blockType{BlockType::Air};
    float hitY{0.5f};       // Y position within the clicked block (0-1) for slab placement
    std::uint8_t face{0};   // Clicked face (0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z)
};

struct TryBreakBlock {
    std::uint32_t seq{0};
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
};

struct TrySetBlock {
    std::uint32_t seq{0};
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
    BlockType blockType{BlockType::Air};
    float hitY{0.5f};
    std::uint8_t face{2};
};

struct BlockPlaced {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
    BlockType blockType{BlockType::Air};
    std::uint8_t stateByte{0};  // BlockState::to_byte() for connections/slab type
};

struct BlockBroken {
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t z{0};
};

struct ActionRejected {
    std::uint32_t seq{0};
    RejectReason reason{RejectReason::Unknown};
};

// ============================================================================
// Messages - Map Export (Editor)
// ============================================================================

struct TryExportMap {
    std::uint32_t seq{0};
    std::string mapId;
    std::uint32_t version{0};
    std::int32_t chunkMinX{0};
    std::int32_t chunkMinZ{0};
    std::int32_t chunkMaxX{0};
    std::int32_t chunkMaxZ{0};
    
    // Environment settings
    std::uint8_t skyboxKind{1};      // 0=None, 1=Day, 2=Night
    float timeOfDayHours{12.0f};
    bool useMoon{false};
    float sunIntensity{1.0f};
    float ambientIntensity{0.25f};
    float temperature{0.5f};
    float humidity{1.0f};
};

struct ExportResult {
    std::uint32_t seq{0};
    bool ok{false};
    RejectReason reason{RejectReason::Unknown};
    std::string path;
};

// ============================================================================
// Messages - Game Events
// ============================================================================

struct TeamAssigned {
    engine::PlayerId playerId{0};
    TeamId teamId{Teams::None};
};

struct HealthUpdate {
    engine::PlayerId playerId{0};
    std::uint8_t hp{20};
    std::uint8_t maxHp{20};
};

struct PlayerDied {
    engine::PlayerId victimId{0};
    engine::PlayerId killerId{0};
    bool isFinalKill{false};
};

struct PlayerRespawned {
    engine::PlayerId playerId{0};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct BedDestroyed {
    TeamId teamId{Teams::None};
    engine::PlayerId destroyerId{0};
};

struct TeamEliminated {
    TeamId teamId{Teams::None};
};

struct MatchEnded {
    TeamId winnerTeamId{Teams::None};
};

// ============================================================================
// Messages - Items
// ============================================================================

struct ItemSpawned {
    std::uint32_t entityId{0};
    ItemType itemType{ItemType::None};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    std::uint16_t count{1};
};

struct ItemPickedUp {
    std::uint32_t entityId{0};
    engine::PlayerId playerId{0};
};

struct InventoryUpdate {
    engine::PlayerId playerId{0};
    ItemType itemType{ItemType::None};
    std::uint16_t count{0};
    std::uint8_t slot{0};
};

// ============================================================================
// Message variant (for type-safe handling)
// ============================================================================

using Message = std::variant<
    // Handshake
    ClientHello,
    ServerHello,
    JoinMatch,
    JoinAck,
    // Input
    InputFrame,
    // State
    StateSnapshot,
    ChunkData,
    // Blocks
    TryPlaceBlock,
    TryBreakBlock,
    TrySetBlock,
    BlockPlaced,
    BlockBroken,
    ActionRejected,
    // Map export
    TryExportMap,
    ExportResult,
    // Game events
    TeamAssigned,
    HealthUpdate,
    PlayerDied,
    PlayerRespawned,
    BedDestroyed,
    TeamEliminated,
    MatchEnded,
    // Items
    ItemSpawned,
    ItemPickedUp,
    InventoryUpdate
>;

} // namespace bedwars::proto
