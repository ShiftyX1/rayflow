#include "enet_common.hpp"

#include <enet/enet.h>

#include <cstring>

namespace shared::transport {

// =============================================================================
// ENetInitializer implementation
// =============================================================================

ENetInitializer::ENetInitializer() {
    initialized_ = (enet_initialize() == 0);
}

ENetInitializer::~ENetInitializer() {
    if (initialized_) {
        enet_deinitialize();
    }
}

// =============================================================================
// Channel/flags helpers
// =============================================================================

ENetChannel get_channel_for_message(const shared::proto::Message& msg) {
    return std::visit([](const auto& m) -> ENetChannel {
        using T = std::decay_t<decltype(m)>;

        if constexpr (std::is_same_v<T, shared::proto::StateSnapshot> ||
                      std::is_same_v<T, shared::proto::InputFrame>) {
            return ENetChannel::Unreliable;
        }
        return ENetChannel::Reliable;
    }, msg);
}

enet_uint32 get_packet_flags_for_message(const shared::proto::Message& msg) {
    ENetChannel channel = get_channel_for_message(msg);
    if (channel == ENetChannel::Unreliable) {
        return ENET_PACKET_FLAG_UNSEQUENCED;
    }
    return ENET_PACKET_FLAG_RELIABLE;
}

// =============================================================================
// Message type indices (must match order in std::variant)
// =============================================================================
enum class MessageTypeIndex : std::uint8_t {
    ClientHello = 0,
    ServerHello = 1,
    JoinMatch = 2,
    JoinAck = 3,
    InputFrame = 4,
    TryPlaceBlock = 5,
    TryBreakBlock = 6,
    TrySetBlock = 7,
    StateSnapshot = 8,
    BlockPlaced = 9,
    BlockBroken = 10,
    ActionRejected = 11,
    TryExportMap = 12,
    ExportResult = 13,
    ChunkData = 14,
};

class BinaryWriter {
public:
    void write_u8(std::uint8_t v) { data_.push_back(v); }
    
    void write_u16(std::uint16_t v) {
        data_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    }
    
    void write_u32(std::uint32_t v) {
        data_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }
    
    void write_u64(std::uint64_t v) {
        write_u32(static_cast<std::uint32_t>(v & 0xFFFFFFFF));
        write_u32(static_cast<std::uint32_t>((v >> 32) & 0xFFFFFFFF));
    }
    
    void write_i32(std::int32_t v) {
        write_u32(static_cast<std::uint32_t>(v));
    }
    
    void write_f32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        write_u32(bits);
    }
    
    void write_bool(bool v) { write_u8(v ? 1 : 0); }
    
    void write_string(const std::string& s) {
        write_u16(static_cast<std::uint16_t>(s.size()));
        for (char c : s) {
            data_.push_back(static_cast<std::uint8_t>(c));
        }
    }
    
    std::vector<std::uint8_t> take() { return std::move(data_); }
    
private:
    std::vector<std::uint8_t> data_;
};

class BinaryReader {
public:
    BinaryReader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size), pos_(0) {}
    
    bool has_remaining(std::size_t n) const { return pos_ + n <= size_; }
    
    bool read_u8(std::uint8_t& v) {
        if (!has_remaining(1)) return false;
        v = data_[pos_++];
        return true;
    }
    
    bool read_u16(std::uint16_t& v) {
        if (!has_remaining(2)) return false;
        v = static_cast<std::uint16_t>(data_[pos_]) |
            (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return true;
    }
    
    bool read_u32(std::uint32_t& v) {
        if (!has_remaining(4)) return false;
        v = static_cast<std::uint32_t>(data_[pos_]) |
            (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
            (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
            (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return true;
    }
    
    bool read_u64(std::uint64_t& v) {
        std::uint32_t lo, hi;
        if (!read_u32(lo) || !read_u32(hi)) return false;
        v = static_cast<std::uint64_t>(lo) | (static_cast<std::uint64_t>(hi) << 32);
        return true;
    }
    
    bool read_i32(std::int32_t& v) {
        std::uint32_t u;
        if (!read_u32(u)) return false;
        v = static_cast<std::int32_t>(u);
        return true;
    }
    
    bool read_f32(float& v) {
        std::uint32_t bits;
        if (!read_u32(bits)) return false;
        std::memcpy(&v, &bits, sizeof(v));
        return true;
    }
    
    bool read_bool(bool& v) {
        std::uint8_t b;
        if (!read_u8(b)) return false;
        v = (b != 0);
        return true;
    }
    
    bool read_string(std::string& s) {
        std::uint16_t len;
        if (!read_u16(len)) return false;
        if (!has_remaining(len)) return false;
        s.assign(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return true;
    }
    
private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
};

std::vector<std::uint8_t> serialize_message(const shared::proto::Message& msg) {
    BinaryWriter w;
    
    std::visit([&w](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        
        if constexpr (std::is_same_v<T, shared::proto::ClientHello>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::ClientHello));
            w.write_u32(m.version);
            w.write_string(m.clientName);
        }
        else if constexpr (std::is_same_v<T, shared::proto::ServerHello>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::ServerHello));
            w.write_u32(m.acceptedVersion);
            w.write_u32(m.tickRate);
            w.write_u32(m.worldSeed);
            w.write_bool(m.hasMapTemplate);
            w.write_string(m.mapId);
            w.write_u32(m.mapVersion);
        }
        else if constexpr (std::is_same_v<T, shared::proto::JoinMatch>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::JoinMatch));
        }
        else if constexpr (std::is_same_v<T, shared::proto::JoinAck>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::JoinAck));
            w.write_u32(m.playerId);
        }
        else if constexpr (std::is_same_v<T, shared::proto::InputFrame>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::InputFrame));
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
        else if constexpr (std::is_same_v<T, shared::proto::TryPlaceBlock>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::TryPlaceBlock));
            w.write_u32(m.seq);
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
            w.write_u8(static_cast<std::uint8_t>(m.blockType));
        }
        else if constexpr (std::is_same_v<T, shared::proto::TryBreakBlock>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::TryBreakBlock));
            w.write_u32(m.seq);
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
        }
        else if constexpr (std::is_same_v<T, shared::proto::TrySetBlock>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::TrySetBlock));
            w.write_u32(m.seq);
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
            w.write_u8(static_cast<std::uint8_t>(m.blockType));
        }
        else if constexpr (std::is_same_v<T, shared::proto::StateSnapshot>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::StateSnapshot));
            w.write_u64(m.serverTick);
            w.write_u32(m.playerId);
            w.write_f32(m.px);
            w.write_f32(m.py);
            w.write_f32(m.pz);
            w.write_f32(m.vx);
            w.write_f32(m.vy);
            w.write_f32(m.vz);
        }
        else if constexpr (std::is_same_v<T, shared::proto::BlockPlaced>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::BlockPlaced));
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
            w.write_u8(static_cast<std::uint8_t>(m.blockType));
        }
        else if constexpr (std::is_same_v<T, shared::proto::BlockBroken>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::BlockBroken));
            w.write_i32(m.x);
            w.write_i32(m.y);
            w.write_i32(m.z);
        }
        else if constexpr (std::is_same_v<T, shared::proto::ActionRejected>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::ActionRejected));
            w.write_u32(m.seq);
            w.write_u8(static_cast<std::uint8_t>(m.reason));
        }
        else if constexpr (std::is_same_v<T, shared::proto::TryExportMap>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::TryExportMap));
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
        else if constexpr (std::is_same_v<T, shared::proto::ExportResult>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::ExportResult));
            w.write_u32(m.seq);
            w.write_bool(m.ok);
            w.write_u8(static_cast<std::uint8_t>(m.reason));
            w.write_string(m.path);
        }
        else if constexpr (std::is_same_v<T, shared::proto::ChunkData>) {
            w.write_u8(static_cast<std::uint8_t>(MessageTypeIndex::ChunkData));
            w.write_i32(m.chunkX);
            w.write_i32(m.chunkZ);
            // Write block count (should be 65536 for 16x256x16)
            w.write_u32(static_cast<std::uint32_t>(m.blocks.size()));
            for (auto b : m.blocks) {
                w.write_u8(b);
            }
        }
    }, msg);
    
    return w.take();
}

bool deserialize_message(const std::uint8_t* data, std::size_t size,
                         shared::proto::Message& outMsg) {
    if (size < 1) return false;
    
    BinaryReader r(data, size);
    
    std::uint8_t typeIndex;
    if (!r.read_u8(typeIndex)) return false;
    
    switch (static_cast<MessageTypeIndex>(typeIndex)) {
        case MessageTypeIndex::ClientHello: {
            shared::proto::ClientHello m;
            if (!r.read_u32(m.version)) return false;
            if (!r.read_string(m.clientName)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::ServerHello: {
            shared::proto::ServerHello m;
            if (!r.read_u32(m.acceptedVersion)) return false;
            if (!r.read_u32(m.tickRate)) return false;
            if (!r.read_u32(m.worldSeed)) return false;
            if (!r.read_bool(m.hasMapTemplate)) return false;
            if (!r.read_string(m.mapId)) return false;
            if (!r.read_u32(m.mapVersion)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::JoinMatch: {
            outMsg = shared::proto::JoinMatch{};
            return true;
        }
        case MessageTypeIndex::JoinAck: {
            shared::proto::JoinAck m;
            if (!r.read_u32(m.playerId)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::InputFrame: {
            shared::proto::InputFrame m;
            if (!r.read_u32(m.seq)) return false;
            if (!r.read_f32(m.moveX)) return false;
            if (!r.read_f32(m.moveY)) return false;
            if (!r.read_f32(m.yaw)) return false;
            if (!r.read_f32(m.pitch)) return false;
            if (!r.read_bool(m.jump)) return false;
            if (!r.read_bool(m.sprint)) return false;
            if (!r.read_bool(m.camUp)) return false;
            if (!r.read_bool(m.camDown)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::TryPlaceBlock: {
            shared::proto::TryPlaceBlock m;
            if (!r.read_u32(m.seq)) return false;
            if (!r.read_i32(m.x)) return false;
            if (!r.read_i32(m.y)) return false;
            if (!r.read_i32(m.z)) return false;
            std::uint8_t bt;
            if (!r.read_u8(bt)) return false;
            m.blockType = static_cast<shared::voxel::BlockType>(bt);
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::TryBreakBlock: {
            shared::proto::TryBreakBlock m;
            if (!r.read_u32(m.seq)) return false;
            if (!r.read_i32(m.x)) return false;
            if (!r.read_i32(m.y)) return false;
            if (!r.read_i32(m.z)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::TrySetBlock: {
            shared::proto::TrySetBlock m;
            if (!r.read_u32(m.seq)) return false;
            if (!r.read_i32(m.x)) return false;
            if (!r.read_i32(m.y)) return false;
            if (!r.read_i32(m.z)) return false;
            std::uint8_t bt;
            if (!r.read_u8(bt)) return false;
            m.blockType = static_cast<shared::voxel::BlockType>(bt);
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::StateSnapshot: {
            shared::proto::StateSnapshot m;
            if (!r.read_u64(m.serverTick)) return false;
            if (!r.read_u32(m.playerId)) return false;
            if (!r.read_f32(m.px)) return false;
            if (!r.read_f32(m.py)) return false;
            if (!r.read_f32(m.pz)) return false;
            if (!r.read_f32(m.vx)) return false;
            if (!r.read_f32(m.vy)) return false;
            if (!r.read_f32(m.vz)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::BlockPlaced: {
            shared::proto::BlockPlaced m;
            if (!r.read_i32(m.x)) return false;
            if (!r.read_i32(m.y)) return false;
            if (!r.read_i32(m.z)) return false;
            std::uint8_t bt;
            if (!r.read_u8(bt)) return false;
            m.blockType = static_cast<shared::voxel::BlockType>(bt);
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::BlockBroken: {
            shared::proto::BlockBroken m;
            if (!r.read_i32(m.x)) return false;
            if (!r.read_i32(m.y)) return false;
            if (!r.read_i32(m.z)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::ActionRejected: {
            shared::proto::ActionRejected m;
            if (!r.read_u32(m.seq)) return false;
            std::uint8_t reason;
            if (!r.read_u8(reason)) return false;
            m.reason = static_cast<shared::proto::RejectReason>(reason);
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::TryExportMap: {
            shared::proto::TryExportMap m;
            if (!r.read_u32(m.seq)) return false;
            if (!r.read_string(m.mapId)) return false;
            if (!r.read_u32(m.version)) return false;
            if (!r.read_i32(m.chunkMinX)) return false;
            if (!r.read_i32(m.chunkMinZ)) return false;
            if (!r.read_i32(m.chunkMaxX)) return false;
            if (!r.read_i32(m.chunkMaxZ)) return false;
            if (!r.read_u8(m.skyboxKind)) return false;
            if (!r.read_f32(m.timeOfDayHours)) return false;
            if (!r.read_bool(m.useMoon)) return false;
            if (!r.read_f32(m.sunIntensity)) return false;
            if (!r.read_f32(m.ambientIntensity)) return false;
            if (!r.read_f32(m.temperature)) return false;
            if (!r.read_f32(m.humidity)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::ExportResult: {
            shared::proto::ExportResult m;
            if (!r.read_u32(m.seq)) return false;
            if (!r.read_bool(m.ok)) return false;
            std::uint8_t reason;
            if (!r.read_u8(reason)) return false;
            m.reason = static_cast<shared::proto::RejectReason>(reason);
            if (!r.read_string(m.path)) return false;
            outMsg = m;
            return true;
        }
        case MessageTypeIndex::ChunkData: {
            shared::proto::ChunkData m;
            if (!r.read_i32(m.chunkX)) return false;
            if (!r.read_i32(m.chunkZ)) return false;
            std::uint32_t blockCount;
            if (!r.read_u32(blockCount)) return false;
            if (!r.has_remaining(blockCount)) return false;
            m.blocks.resize(blockCount);
            for (std::uint32_t i = 0; i < blockCount; ++i) {
                if (!r.read_u8(m.blocks[i])) return false;
            }
            outMsg = m;
            return true;
        }
        default:
            return false;
    }
}

} // namespace shared::transport
