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

    // MT-1: optional finite map template loaded by the server.
    // When true, client may try to load `maps/<mapId>_v<mapVersion>.rfmap` locally.
    bool hasMapTemplate{false};
    std::string mapId;
    std::uint32_t mapVersion{0};
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

    // Editor camera mode (map editor): vertical movement controls.
    // These are ignored by the normal physics-based movement.
    bool camUp{false};
    bool camDown{false};
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

// Client -> Server: editor intent to set a block (authoritative server validates/applies).
// Unlike TryPlaceBlock/TryBreakBlock, this is intended for tools (map editor).
struct TrySetBlock {
    std::uint32_t seq{0};
    int x{0};
    int y{0};
    int z{0};
    shared::voxel::BlockType blockType{shared::voxel::BlockType::Air};
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

// Client -> Server: request server-side export of a finite map template.
// Bounds are provided in chunk coordinates (inclusive).
struct TryExportMap {
    std::uint32_t seq{0};
    std::string mapId;
    std::uint32_t version{0};
    int chunkMinX{0};
    int chunkMinZ{0};
    int chunkMaxX{0};
    int chunkMaxZ{0};

    // MV-1: render-only environment settings embedded into the exported template.
    // Keep as plain fields to avoid cross-layer dependencies.
    // MV-1: 0=None, 1=Day, 2=Night. Extended: values >2 select Panorama_Sky_XX by numeric id.
    std::uint8_t skyboxKind{1};
    float timeOfDayHours{12.0f};
    bool useMoon{false};
    float sunIntensity{1.0f};
    float ambientIntensity{0.25f};

    // MV-2: global temperature used for grass/foliage tint in rendering.
    // Range: [0, 1] where 0=cold, 1=hot.
    float temperature{0.5f};
};

// Server -> Client: result of TryExportMap.
struct ExportResult {
    std::uint32_t seq{0};
    bool ok{false};
    RejectReason reason{RejectReason::Unknown};
    std::string path;
};

using Message = std::variant<
    ClientHello,
    ServerHello,
    JoinMatch,
    JoinAck,
    InputFrame,
    TryPlaceBlock,
    TryBreakBlock,
    TrySetBlock,
    StateSnapshot,
    BlockPlaced,
    BlockBroken,
    ActionRejected,
    TryExportMap,
    ExportResult
>;

} // namespace shared::proto
