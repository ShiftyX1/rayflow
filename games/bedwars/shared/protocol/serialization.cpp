#include "serialization.hpp"

namespace bedwars::proto {

// ============================================================================
// Serialize
// ============================================================================

std::vector<std::uint8_t> serialize(const Message& msg) {
    engine::ByteWriter w;
    
    std::visit([&w](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        
        // --- Handshake ---
        if constexpr (std::is_same_v<T, ClientHello>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ClientHello));
            w.write_u32(m.version);
            w.write_string(m.clientName);
        }
        else if constexpr (std::is_same_v<T, ServerHello>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ServerHello));
            w.write_u32(m.acceptedVersion);
            w.write_u32(m.tickRate);
            w.write_u32(m.worldSeed);
            w.write_bool(m.hasMapTemplate);
            w.write_string(m.mapId);
            w.write_u32(m.mapVersion);
        }
        else if constexpr (std::is_same_v<T, JoinMatch>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::JoinMatch));
        }
        else if constexpr (std::is_same_v<T, JoinAck>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::JoinAck));
            w.write_u32(m.playerId);
        }
        // --- Input ---
        else if constexpr (std::is_same_v<T, InputFrame>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::InputFrame));
            w.write_u32(m.seq);
            w.write_f32(m.moveX);
            w.write_f32(m.moveY);
            w.write_f32(m.yaw);
            w.write_f32(m.pitch);
            w.write_bool(m.jump);
            w.write_bool(m.sprint);
            w.write_bool(m.camUp);
            w.write_bool(m.camDown);
        }
        // --- State ---
        else if constexpr (std::is_same_v<T, StateSnapshot>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::StateSnapshot));
            w.write_u64(m.serverTick);
            w.write_u32(m.playerId);
            w.write_f32(m.px);
            w.write_f32(m.py);
            w.write_f32(m.pz);
            w.write_f32(m.vx);
            w.write_f32(m.vy);
            w.write_f32(m.vz);
        }
        else if constexpr (std::is_same_v<T, ChunkData>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ChunkData));
            w.write_i32(m.chunkX);
            w.write_i32(m.chunkZ);
            w.write_u32(static_cast<std::uint32_t>(m.blocks.size()));
            for (auto b : m.blocks) {
                w.write_u8(b);
            }
        }
        // --- Blocks ---
        else if constexpr (std::is_same_v<T, TryPlaceBlock>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::TryPlaceBlock));
            w.write_u32(m.seq);
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
            w.write_u8(static_cast<std::uint8_t>(m.blockType));
            w.write_f32(m.hitY);
            w.write_u8(m.face);
        }
        else if constexpr (std::is_same_v<T, TryBreakBlock>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::TryBreakBlock));
            w.write_u32(m.seq);
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
        }
        else if constexpr (std::is_same_v<T, TrySetBlock>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::TrySetBlock));
            w.write_u32(m.seq);
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
            w.write_u8(static_cast<std::uint8_t>(m.blockType));
            w.write_f32(m.hitY);
            w.write_u8(m.face);
        }
        else if constexpr (std::is_same_v<T, BlockPlaced>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::BlockPlaced));
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
            w.write_u8(static_cast<std::uint8_t>(m.blockType));
            w.write_u8(m.stateByte);
        }
        else if constexpr (std::is_same_v<T, BlockBroken>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::BlockBroken));
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
        }
        else if constexpr (std::is_same_v<T, ActionRejected>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ActionRejected));
            w.write_u32(m.seq);
            w.write_u8(static_cast<std::uint8_t>(m.reason));
        }
        // --- Map Export ---
        else if constexpr (std::is_same_v<T, TryExportMap>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::TryExportMap));
            w.write_u32(m.seq);
            w.write_string(m.mapId);
            w.write_u32(m.version);
            w.write_i32(m.chunkMinX);
            w.write_i32(m.chunkMinZ);
            w.write_i32(m.chunkMaxX);
            w.write_i32(m.chunkMaxZ);
            w.write_u8(m.skyboxKind);
            w.write_f32(m.timeOfDayHours);
            w.write_bool(m.useMoon);
            w.write_f32(m.sunIntensity);
            w.write_f32(m.ambientIntensity);
            w.write_f32(m.temperature);
            w.write_f32(m.humidity);
        }
        else if constexpr (std::is_same_v<T, ExportResult>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ExportResult));
            w.write_u32(m.seq);
            w.write_bool(m.ok);
            w.write_u8(static_cast<std::uint8_t>(m.reason));
            w.write_string(m.path);
        }
        // --- Game Events ---
        else if constexpr (std::is_same_v<T, TeamAssigned>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::TeamAssigned));
            w.write_u32(m.playerId);
            w.write_u8(m.teamId);
        }
        else if constexpr (std::is_same_v<T, HealthUpdate>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::HealthUpdate));
            w.write_u32(m.playerId);
            w.write_u8(m.hp);
            w.write_u8(m.maxHp);
        }
        else if constexpr (std::is_same_v<T, PlayerDied>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::PlayerDied));
            w.write_u32(m.victimId);
            w.write_u32(m.killerId);
            w.write_bool(m.isFinalKill);
        }
        else if constexpr (std::is_same_v<T, PlayerRespawned>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::PlayerRespawned));
            w.write_u32(m.playerId);
            w.write_f32(m.x);
            w.write_f32(m.y);
            w.write_f32(m.z);
        }
        else if constexpr (std::is_same_v<T, BedDestroyed>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::BedDestroyed));
            w.write_u8(m.teamId);
            w.write_u32(m.destroyerId);
        }
        else if constexpr (std::is_same_v<T, TeamEliminated>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::TeamEliminated));
            w.write_u8(m.teamId);
        }
        else if constexpr (std::is_same_v<T, MatchEnded>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::MatchEnded));
            w.write_u8(m.winnerTeamId);
        }
        // --- Items ---
        else if constexpr (std::is_same_v<T, ItemSpawned>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ItemSpawned));
            w.write_u32(m.entityId);
            w.write_u16(static_cast<std::uint16_t>(m.itemType));
            w.write_f32(m.x);
            w.write_f32(m.y);
            w.write_f32(m.z);
            w.write_u16(m.count);
        }
        else if constexpr (std::is_same_v<T, ItemPickedUp>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::ItemPickedUp));
            w.write_u32(m.entityId);
            w.write_u32(m.playerId);
        }
        else if constexpr (std::is_same_v<T, InventoryUpdate>) {
            w.write_u8(static_cast<std::uint8_t>(MessageType::InventoryUpdate));
            w.write_u32(m.playerId);
            w.write_u16(static_cast<std::uint16_t>(m.itemType));
            w.write_u16(m.count);
            w.write_u8(m.slot);
        }
    }, msg);
    
    return w.take();
}

// ============================================================================
// Deserialize
// ============================================================================

std::optional<Message> deserialize(std::span<const std::uint8_t> data) {
    if (data.empty()) return std::nullopt;
    
    try {
        engine::ByteReader r(data);
        auto type = static_cast<MessageType>(r.read_u8());
        
        switch (type) {
            // --- Handshake ---
            case MessageType::ClientHello: {
                ClientHello m;
                m.version = r.read_u32();
                m.clientName = r.read_string();
                return m;
            }
            case MessageType::ServerHello: {
                ServerHello m;
                m.acceptedVersion = r.read_u32();
                m.tickRate = r.read_u32();
                m.worldSeed = r.read_u32();
                m.hasMapTemplate = r.read_bool();
                m.mapId = r.read_string();
                m.mapVersion = r.read_u32();
                return m;
            }
            case MessageType::JoinMatch: {
                return JoinMatch{};
            }
            case MessageType::JoinAck: {
                JoinAck m;
                m.playerId = r.read_u32();
                return m;
            }
            // --- Input ---
            case MessageType::InputFrame: {
                InputFrame m;
                m.seq = r.read_u32();
                m.moveX = r.read_f32();
                m.moveY = r.read_f32();
                m.yaw = r.read_f32();
                m.pitch = r.read_f32();
                m.jump = r.read_bool();
                m.sprint = r.read_bool();
                m.camUp = r.read_bool();
                m.camDown = r.read_bool();
                return m;
            }
            // --- State ---
            case MessageType::StateSnapshot: {
                StateSnapshot m;
                m.serverTick = r.read_u64();
                m.playerId = r.read_u32();
                m.px = r.read_f32();
                m.py = r.read_f32();
                m.pz = r.read_f32();
                m.vx = r.read_f32();
                m.vy = r.read_f32();
                m.vz = r.read_f32();
                return m;
            }
            case MessageType::ChunkData: {
                ChunkData m;
                m.chunkX = r.read_i32();
                m.chunkZ = r.read_i32();
                std::uint32_t size = r.read_u32();
                m.blocks.resize(size);
                for (std::uint32_t i = 0; i < size; ++i) {
                    m.blocks[i] = r.read_u8();
                }
                return m;
            }
            // --- Blocks ---
            case MessageType::TryPlaceBlock: {
                TryPlaceBlock m;
                m.seq = r.read_u32();
                m.x = r.read_i32();
                m.y = r.read_i32();
                m.z = r.read_i32();
                m.blockType = static_cast<BlockType>(r.read_u8());
                m.hitY = r.read_f32();
                m.face = r.read_u8();
                return m;
            }
            case MessageType::TryBreakBlock: {
                TryBreakBlock m;
                m.seq = r.read_u32();
                m.x = r.read_i32();
                m.y = r.read_i32();
                m.z = r.read_i32();
                return m;
            }
            case MessageType::TrySetBlock: {
                TrySetBlock m;
                m.seq = r.read_u32();
                m.x = r.read_i32();
                m.y = r.read_i32();
                m.z = r.read_i32();
                m.blockType = static_cast<BlockType>(r.read_u8());
                m.hitY = r.read_f32();
                m.face = r.read_u8();
                return m;
            }
            case MessageType::BlockPlaced: {
                BlockPlaced m;
                m.x = r.read_i32();
                m.y = r.read_i32();
                m.z = r.read_i32();
                m.blockType = static_cast<BlockType>(r.read_u8());
                m.stateByte = r.read_u8();
                return m;
            }
            case MessageType::BlockBroken: {
                BlockBroken m;
                m.x = r.read_i32();
                m.y = r.read_i32();
                m.z = r.read_i32();
                return m;
            }
            case MessageType::ActionRejected: {
                ActionRejected m;
                m.seq = r.read_u32();
                m.reason = static_cast<RejectReason>(r.read_u8());
                return m;
            }
            // --- Map Export ---
            case MessageType::TryExportMap: {
                TryExportMap m;
                m.seq = r.read_u32();
                m.mapId = r.read_string();
                m.version = r.read_u32();
                m.chunkMinX = r.read_i32();
                m.chunkMinZ = r.read_i32();
                m.chunkMaxX = r.read_i32();
                m.chunkMaxZ = r.read_i32();
                m.skyboxKind = r.read_u8();
                m.timeOfDayHours = r.read_f32();
                m.useMoon = r.read_bool();
                m.sunIntensity = r.read_f32();
                m.ambientIntensity = r.read_f32();
                m.temperature = r.read_f32();
                m.humidity = r.read_f32();
                return m;
            }
            case MessageType::ExportResult: {
                ExportResult m;
                m.seq = r.read_u32();
                m.ok = r.read_bool();
                m.reason = static_cast<RejectReason>(r.read_u8());
                m.path = r.read_string();
                return m;
            }
            // --- Game Events ---
            case MessageType::TeamAssigned: {
                TeamAssigned m;
                m.playerId = r.read_u32();
                m.teamId = r.read_u8();
                return m;
            }
            case MessageType::HealthUpdate: {
                HealthUpdate m;
                m.playerId = r.read_u32();
                m.hp = r.read_u8();
                m.maxHp = r.read_u8();
                return m;
            }
            case MessageType::PlayerDied: {
                PlayerDied m;
                m.victimId = r.read_u32();
                m.killerId = r.read_u32();
                m.isFinalKill = r.read_bool();
                return m;
            }
            case MessageType::PlayerRespawned: {
                PlayerRespawned m;
                m.playerId = r.read_u32();
                m.x = r.read_f32();
                m.y = r.read_f32();
                m.z = r.read_f32();
                return m;
            }
            case MessageType::BedDestroyed: {
                BedDestroyed m;
                m.teamId = r.read_u8();
                m.destroyerId = r.read_u32();
                return m;
            }
            case MessageType::TeamEliminated: {
                TeamEliminated m;
                m.teamId = r.read_u8();
                return m;
            }
            case MessageType::MatchEnded: {
                MatchEnded m;
                m.winnerTeamId = r.read_u8();
                return m;
            }
            // --- Items ---
            case MessageType::ItemSpawned: {
                ItemSpawned m;
                m.entityId = r.read_u32();
                m.itemType = static_cast<ItemType>(r.read_u16());
                m.x = r.read_f32();
                m.y = r.read_f32();
                m.z = r.read_f32();
                m.count = r.read_u16();
                return m;
            }
            case MessageType::ItemPickedUp: {
                ItemPickedUp m;
                m.entityId = r.read_u32();
                m.playerId = r.read_u32();
                return m;
            }
            case MessageType::InventoryUpdate: {
                InventoryUpdate m;
                m.playerId = r.read_u32();
                m.itemType = static_cast<ItemType>(r.read_u16());
                m.count = r.read_u16();
                m.slot = r.read_u8();
                return m;
            }
            default:
                return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace bedwars::proto
