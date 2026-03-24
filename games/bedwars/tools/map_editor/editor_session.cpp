#include "editor_session.hpp"
#include <games/bedwars/shared/protocol/serialization.hpp>

#include <raylib.h>  // For TraceLog

namespace editor {

EditorSession::EditorSession(std::shared_ptr<engine::transport::IClientTransport> transport)
    : transport_(std::move(transport)) 
{
    // Setup receive callback
    transport_->onReceive = [this](std::span<const std::uint8_t> data) {
        auto msg = proto::deserialize(data);
        if (msg) {
            handle_message(*msg);
        }
    };
}

void EditorSession::start_handshake(const std::string& clientName) {
    TraceLog(LOG_INFO, "[editor] Starting handshake as '%s'", clientName.c_str());
    
    proto::ClientHello hello;
    hello.version = proto::kProtocolVersion;
    hello.clientName = clientName;
    send_message(hello);
    
    // Send JoinMatch immediately (like legacy ClientSession)
    proto::JoinMatch join;
    send_message(join);
    TraceLog(LOG_INFO, "[editor] Sent ClientHello + JoinMatch");
}

void EditorSession::poll() {
    if (transport_) {
        transport_->poll(0);
    }
}

bool EditorSession::is_connected() const {
    return serverHello_.has_value();
}

void EditorSession::send_input(float moveX, float moveY, float yaw, float pitch,
                                bool jump, bool sprint, bool camUp, bool camDown) {
    proto::InputFrame input;
    input.seq = inputSeq_++;
    input.moveX = moveX;
    input.moveY = moveY;
    input.yaw = yaw;
    input.pitch = pitch;
    input.jump = jump;
    input.sprint = sprint;
    input.camUp = camUp;
    input.camDown = camDown;
    send_message(input);
}

void EditorSession::send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType,
                                        float hitY, std::uint8_t face) {
    proto::TrySetBlock msg;
    msg.seq = actionSeq_++;
    msg.x = x;
    msg.y = y;
    msg.z = z;
    msg.blockType = blockType;
    msg.hitY = hitY;
    msg.face = face;
    TraceLog(LOG_DEBUG, "[editor] TrySetBlock seq=%u pos=(%d,%d,%d) type=%d", 
             msg.seq, x, y, z, static_cast<int>(blockType));
    send_message(msg);
}

void EditorSession::send_try_export_map(const std::string& mapId,
                                         std::uint32_t version,
                                         int chunkMinX, int chunkMinZ,
                                         int chunkMaxX, int chunkMaxZ,
                                         std::uint8_t skyboxKind,
                                         float timeOfDayHours,
                                         bool useMoon,
                                         float sunIntensity,
                                         float ambientIntensity,
                                         float temperature,
                                         float humidity) {
    proto::TryExportMap msg;
    msg.seq = actionSeq_++;
    msg.mapId = mapId;
    msg.version = version;
    msg.chunkMinX = chunkMinX;
    msg.chunkMinZ = chunkMinZ;
    msg.chunkMaxX = chunkMaxX;
    msg.chunkMaxZ = chunkMaxZ;
    msg.skyboxKind = skyboxKind;
    msg.timeOfDayHours = timeOfDayHours;
    msg.useMoon = useMoon;
    msg.sunIntensity = sunIntensity;
    msg.ambientIntensity = ambientIntensity;
    msg.temperature = temperature;
    msg.humidity = humidity;
    send_message(msg);
}

void EditorSession::send_message(const proto::Message& msg) {
    auto data = proto::serialize(msg);
    TraceLog(LOG_DEBUG, "[editor] Sending message (%zu bytes)", data.size());
    transport_->send(data);
}

void EditorSession::handle_message(const proto::Message& msg) {
    std::visit([this](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        
        if constexpr (std::is_same_v<T, proto::ServerHello>) {
            serverHello_ = m;
            TraceLog(LOG_INFO, "[editor] ServerHello: tickRate=%u seed=%u", m.tickRate, m.worldSeed);
        }
        else if constexpr (std::is_same_v<T, proto::JoinAck>) {
            joinAck_ = m;
            TraceLog(LOG_INFO, "[editor] JoinAck: playerId=%u", m.playerId);
        }
        else if constexpr (std::is_same_v<T, proto::StateSnapshot>) {
            latestSnapshot_ = m;
        }
        else if constexpr (std::is_same_v<T, proto::BlockPlaced>) {
            if (onBlockPlaced_) onBlockPlaced_(m);
        }
        else if constexpr (std::is_same_v<T, proto::BlockBroken>) {
            if (onBlockBroken_) onBlockBroken_(m);
        }
        else if constexpr (std::is_same_v<T, proto::ActionRejected>) {
            TraceLog(LOG_WARNING, "[editor] ActionRejected: seq=%u reason=%u", m.seq, static_cast<unsigned>(m.reason));
            if (onActionRejected_) onActionRejected_(m);
        }
        else if constexpr (std::is_same_v<T, proto::ExportResult>) {
            TraceLog(LOG_INFO, "[editor] ExportResult: seq=%u ok=%d path=%s", 
                     m.seq, m.ok ? 1 : 0, m.path.c_str());
            if (onExportResult_) onExportResult_(m);
        }
    }, msg);
}

} // namespace editor
