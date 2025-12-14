#include "client_session.hpp"

#include <raylib.h>

namespace client::net {

ClientSession::ClientSession(std::shared_ptr<shared::transport::IEndpoint> endpoint)
    : endpoint_(std::move(endpoint)) {}

void ClientSession::start_handshake() {
    shared::proto::ClientHello hello;
    hello.version = shared::proto::kProtocolVersion;
    hello.clientName = "local-client";
    endpoint_->send(std::move(hello));

    endpoint_->send(shared::proto::JoinMatch{});
}

void ClientSession::send_input(float moveX, float moveY, float yaw, float pitch, bool jump, bool sprint) {
    shared::proto::InputFrame frame;
    frame.seq = ++inputSeq_;
    frame.moveX = moveX;
    frame.moveY = moveY;
    frame.yaw = yaw;
    frame.pitch = pitch;
    frame.jump = jump;
    frame.sprint = sprint;

    endpoint_->send(std::move(frame));
}

void ClientSession::send_try_break_block(int x, int y, int z) {
    shared::proto::TryBreakBlock req;
    req.seq = ++actionSeq_;
    req.x = x;
    req.y = y;
    req.z = z;
    endpoint_->send(std::move(req));
}

void ClientSession::send_try_place_block(int x, int y, int z, shared::voxel::BlockType blockType) {
    shared::proto::TryPlaceBlock req;
    req.seq = ++actionSeq_;
    req.x = x;
    req.y = y;
    req.z = z;
    req.blockType = blockType;
    endpoint_->send(std::move(req));
}

void ClientSession::send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType) {
    shared::proto::TrySetBlock req;
    req.seq = ++actionSeq_;
    req.x = x;
    req.y = y;
    req.z = z;
    req.blockType = blockType;
    endpoint_->send(std::move(req));
}

void ClientSession::send_try_export_map(const std::string& mapId,
                                       std::uint32_t version,
                                       int chunkMinX,
                                       int chunkMinZ,
                                       int chunkMaxX,
                                       int chunkMaxZ,
                                       std::uint8_t skyboxKind,
                                       float timeOfDayHours,
                                       bool useMoon,
                                       float sunIntensity,
                                       float ambientIntensity) {
    shared::proto::TryExportMap req;
    req.seq = ++actionSeq_;
    req.mapId = mapId;
    req.version = version;
    req.chunkMinX = chunkMinX;
    req.chunkMinZ = chunkMinZ;
    req.chunkMaxX = chunkMaxX;
    req.chunkMaxZ = chunkMaxZ;
    req.skyboxKind = skyboxKind;
    req.timeOfDayHours = timeOfDayHours;
    req.useMoon = useMoon;
    req.sunIntensity = sunIntensity;
    req.ambientIntensity = ambientIntensity;
    endpoint_->send(std::move(req));
}

void ClientSession::poll() {
    shared::proto::Message msg;
    while (endpoint_->try_recv(msg)) {
        if (std::holds_alternative<shared::proto::ServerHello>(msg)) {
            serverHello_ = std::get<shared::proto::ServerHello>(msg);
            TraceLog(LOG_INFO, "[net] ServerHello: tickRate=%u worldSeed=%u", serverHello_->tickRate, serverHello_->worldSeed);
        } else if (std::holds_alternative<shared::proto::JoinAck>(msg)) {
            joinAck_ = std::get<shared::proto::JoinAck>(msg);
            TraceLog(LOG_INFO, "[net] JoinAck: playerId=%u", joinAck_->playerId);
        } else if (std::holds_alternative<shared::proto::StateSnapshot>(msg)) {
            latestSnapshot_ = std::get<shared::proto::StateSnapshot>(msg);
        } else if (std::holds_alternative<shared::proto::BlockPlaced>(msg)) {
            const auto& ev = std::get<shared::proto::BlockPlaced>(msg);
            if (onBlockPlaced_) onBlockPlaced_(ev);
        } else if (std::holds_alternative<shared::proto::BlockBroken>(msg)) {
            const auto& ev = std::get<shared::proto::BlockBroken>(msg);
            if (onBlockBroken_) onBlockBroken_(ev);
        } else if (std::holds_alternative<shared::proto::ActionRejected>(msg)) {
            const auto& rej = std::get<shared::proto::ActionRejected>(msg);
            TraceLog(LOG_WARNING, "[net] ActionRejected: seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            if (onActionRejected_) onActionRejected_(rej);
        }
    }
}

} // namespace client::net
