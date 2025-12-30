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

void ClientSession::reset() {
    inputSeq_ = 0;
    actionSeq_ = 0;
    serverHello_.reset();
    joinAck_.reset();
    latestSnapshot_.reset();
    // Note: callbacks are kept intact for reuse
}

void ClientSession::send_input(float moveX, float moveY, float yaw, float pitch, bool jump, bool sprint, bool camUp, bool camDown) {
    shared::proto::InputFrame frame;
    frame.seq = ++inputSeq_;
    frame.moveX = moveX;
    frame.moveY = moveY;
    frame.yaw = yaw;
    frame.pitch = pitch;
    frame.jump = jump;
    frame.sprint = sprint;
    frame.camUp = camUp;
    frame.camDown = camDown;

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

void ClientSession::send_try_place_block(int x, int y, int z, shared::voxel::BlockType blockType, float hitY, std::uint8_t face) {
    shared::proto::TryPlaceBlock req;
    req.seq = ++actionSeq_;
    req.x = x;
    req.y = y;
    req.z = z;
    req.blockType = blockType;
    req.hitY = hitY;
    req.face = face;
    endpoint_->send(std::move(req));
}

void ClientSession::send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType, float hitY, std::uint8_t face) {
    shared::proto::TrySetBlock req;
    req.seq = ++actionSeq_;
    req.x = x;
    req.y = y;
    req.z = z;
    req.blockType = blockType;
    req.hitY = hitY;
    req.face = face;
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
                                       float ambientIntensity,
                                       float temperature,
                                       float humidity) {
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
    req.temperature = temperature;
    req.humidity = humidity;
    endpoint_->send(std::move(req));
}

void ClientSession::poll() {
    shared::proto::Message msg;
    while (endpoint_->try_recv(msg)) {
        if (std::holds_alternative<shared::proto::ServerHello>(msg)) {
            serverHello_ = std::get<shared::proto::ServerHello>(msg);
            TraceLog(LOG_INFO, "[net] ServerHello: tickRate=%u worldSeed=%u hasMap=%d mapId=%s mapVer=%u",
                     serverHello_->tickRate,
                     serverHello_->worldSeed,
                     serverHello_->hasMapTemplate ? 1 : 0,
                     serverHello_->mapId.c_str(),
                     serverHello_->mapVersion);
        } else if (std::holds_alternative<shared::proto::JoinAck>(msg)) {
            joinAck_ = std::get<shared::proto::JoinAck>(msg);
            TraceLog(LOG_INFO, "[net] JoinAck: playerId=%u", joinAck_->playerId);
        } else if (std::holds_alternative<shared::proto::StateSnapshot>(msg)) {
            latestSnapshot_ = std::get<shared::proto::StateSnapshot>(msg);
        } else if (std::holds_alternative<shared::proto::BlockPlaced>(msg)) {
            const auto& ev = std::get<shared::proto::BlockPlaced>(msg);
            pendingBlockPlaced_.push_back(ev);
            if (onBlockPlaced_) onBlockPlaced_(ev);
        } else if (std::holds_alternative<shared::proto::BlockBroken>(msg)) {
            const auto& ev = std::get<shared::proto::BlockBroken>(msg);
            pendingBlockBroken_.push_back(ev);
            if (onBlockBroken_) onBlockBroken_(ev);
        } else if (std::holds_alternative<shared::proto::ChunkData>(msg)) {
            const auto& cd = std::get<shared::proto::ChunkData>(msg);
            pendingChunkData_.push_back(cd);
            if (onChunkData_) onChunkData_(cd);
        } else if (std::holds_alternative<shared::proto::ActionRejected>(msg)) {
            const auto& rej = std::get<shared::proto::ActionRejected>(msg);
            TraceLog(LOG_WARNING, "[net] ActionRejected: seq=%u reason=%u", rej.seq, static_cast<unsigned>(rej.reason));
            if (onActionRejected_) onActionRejected_(rej);
        } else if (std::holds_alternative<shared::proto::ExportResult>(msg)) {
            const auto& ev = std::get<shared::proto::ExportResult>(msg);
            TraceLog(LOG_INFO, "[net] ExportResult: seq=%u ok=%d reason=%u path=%s",
                     ev.seq,
                     ev.ok ? 1 : 0,
                     static_cast<unsigned>(ev.reason),
                     ev.path.c_str());
            if (onExportResult_) onExportResult_(ev);
        }
        // === Game Events ===
        else if (std::holds_alternative<shared::proto::TeamAssigned>(msg)) {
            const auto& ev = std::get<shared::proto::TeamAssigned>(msg);
            TraceLog(LOG_INFO, "[net] TeamAssigned: playerId=%u teamId=%u (%s)",
                     ev.playerId, ev.teamId, shared::game::team_name(ev.teamId));
            if (onTeamAssigned_) onTeamAssigned_(ev);
        } else if (std::holds_alternative<shared::proto::HealthUpdate>(msg)) {
            const auto& ev = std::get<shared::proto::HealthUpdate>(msg);
            TraceLog(LOG_DEBUG, "[net] HealthUpdate: playerId=%u hp=%u/%u",
                     ev.playerId, ev.hp, ev.maxHp);
            if (onHealthUpdate_) onHealthUpdate_(ev);
        } else if (std::holds_alternative<shared::proto::PlayerDied>(msg)) {
            const auto& ev = std::get<shared::proto::PlayerDied>(msg);
            TraceLog(LOG_INFO, "[net] PlayerDied: victimId=%u killerId=%u finalKill=%d",
                     ev.victimId, ev.killerId, ev.isFinalKill ? 1 : 0);
            if (onPlayerDied_) onPlayerDied_(ev);
        } else if (std::holds_alternative<shared::proto::PlayerRespawned>(msg)) {
            const auto& ev = std::get<shared::proto::PlayerRespawned>(msg);
            TraceLog(LOG_INFO, "[net] PlayerRespawned: playerId=%u pos=(%.1f,%.1f,%.1f)",
                     ev.playerId, ev.x, ev.y, ev.z);
            if (onPlayerRespawned_) onPlayerRespawned_(ev);
        } else if (std::holds_alternative<shared::proto::BedDestroyed>(msg)) {
            const auto& ev = std::get<shared::proto::BedDestroyed>(msg);
            TraceLog(LOG_INFO, "[net] BedDestroyed: teamId=%u (%s) destroyerId=%u",
                     ev.teamId, shared::game::team_name(ev.teamId), ev.destroyerId);
            if (onBedDestroyed_) onBedDestroyed_(ev);
        } else if (std::holds_alternative<shared::proto::TeamEliminated>(msg)) {
            const auto& ev = std::get<shared::proto::TeamEliminated>(msg);
            TraceLog(LOG_INFO, "[net] TeamEliminated: teamId=%u (%s)",
                     ev.teamId, shared::game::team_name(ev.teamId));
            if (onTeamEliminated_) onTeamEliminated_(ev);
        } else if (std::holds_alternative<shared::proto::MatchEnded>(msg)) {
            const auto& ev = std::get<shared::proto::MatchEnded>(msg);
            TraceLog(LOG_INFO, "[net] MatchEnded: winnerTeamId=%u (%s)",
                     ev.winnerTeamId, shared::game::team_name(ev.winnerTeamId));
            if (onMatchEnded_) onMatchEnded_(ev);
        } else if (std::holds_alternative<shared::proto::ItemSpawned>(msg)) {
            const auto& ev = std::get<shared::proto::ItemSpawned>(msg);
            TraceLog(LOG_DEBUG, "[net] ItemSpawned: entityId=%u type=%u pos=(%.1f,%.1f,%.1f) count=%u",
                     ev.entityId, static_cast<unsigned>(ev.itemType), ev.x, ev.y, ev.z, ev.count);
            if (onItemSpawned_) onItemSpawned_(ev);
        } else if (std::holds_alternative<shared::proto::ItemPickedUp>(msg)) {
            const auto& ev = std::get<shared::proto::ItemPickedUp>(msg);
            TraceLog(LOG_DEBUG, "[net] ItemPickedUp: entityId=%u playerId=%u",
                     ev.entityId, ev.playerId);
            if (onItemPickedUp_) onItemPickedUp_(ev);
        } else if (std::holds_alternative<shared::proto::InventoryUpdate>(msg)) {
            const auto& ev = std::get<shared::proto::InventoryUpdate>(msg);
            TraceLog(LOG_DEBUG, "[net] InventoryUpdate: playerId=%u type=%u count=%u slot=%u",
                     ev.playerId, static_cast<unsigned>(ev.itemType), ev.count, ev.slot);
            if (onInventoryUpdate_) onInventoryUpdate_(ev);
        }
    }
}

std::vector<shared::proto::BlockPlaced> ClientSession::take_pending_block_placed() {
    return std::move(pendingBlockPlaced_);
}

std::vector<shared::proto::BlockBroken> ClientSession::take_pending_block_broken() {
    return std::move(pendingBlockBroken_);
}

std::vector<shared::proto::ChunkData> ClientSession::take_pending_chunk_data() {
    return std::move(pendingChunkData_);
}

} // namespace client::net
