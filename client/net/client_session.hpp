#pragma once

#include "../../shared/transport/endpoint.hpp"
#include "../../shared/game/team_types.hpp"
#include "../../shared/game/item_types.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace client::net {

class ClientSession {
public:
    explicit ClientSession(std::shared_ptr<shared::transport::IEndpoint> endpoint);

    void start_handshake();
    void poll();
    void reset();  // Reset session state for reconnect

    void send_input(float moveX, float moveY, float yaw, float pitch, bool jump, bool sprint, bool camUp = false, bool camDown = false);

    void send_try_break_block(int x, int y, int z);
    void send_try_place_block(int x, int y, int z, shared::voxel::BlockType blockType, float hitY = 0.5f, std::uint8_t face = 0);
    void send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType, float hitY = 0.5f, std::uint8_t face = 2);
    void send_try_export_map(const std::string& mapId,
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
                             float humidity);

    // Block event callbacks
    void set_on_block_placed(std::function<void(const shared::proto::BlockPlaced&)> cb) { onBlockPlaced_ = std::move(cb); }
    void set_on_block_broken(std::function<void(const shared::proto::BlockBroken&)> cb) { onBlockBroken_ = std::move(cb); }
    void set_on_action_rejected(std::function<void(const shared::proto::ActionRejected&)> cb) { onActionRejected_ = std::move(cb); }
    void set_on_export_result(std::function<void(const shared::proto::ExportResult&)> cb) { onExportResult_ = std::move(cb); }
    void set_on_chunk_data(std::function<void(const shared::proto::ChunkData&)> cb) { onChunkData_ = std::move(cb); }

    // Game event callbacks
    void set_on_team_assigned(std::function<void(const shared::proto::TeamAssigned&)> cb) { onTeamAssigned_ = std::move(cb); }
    void set_on_health_update(std::function<void(const shared::proto::HealthUpdate&)> cb) { onHealthUpdate_ = std::move(cb); }
    void set_on_player_died(std::function<void(const shared::proto::PlayerDied&)> cb) { onPlayerDied_ = std::move(cb); }
    void set_on_player_respawned(std::function<void(const shared::proto::PlayerRespawned&)> cb) { onPlayerRespawned_ = std::move(cb); }
    void set_on_bed_destroyed(std::function<void(const shared::proto::BedDestroyed&)> cb) { onBedDestroyed_ = std::move(cb); }
    void set_on_team_eliminated(std::function<void(const shared::proto::TeamEliminated&)> cb) { onTeamEliminated_ = std::move(cb); }
    void set_on_match_ended(std::function<void(const shared::proto::MatchEnded&)> cb) { onMatchEnded_ = std::move(cb); }
    void set_on_item_spawned(std::function<void(const shared::proto::ItemSpawned&)> cb) { onItemSpawned_ = std::move(cb); }
    void set_on_item_picked_up(std::function<void(const shared::proto::ItemPickedUp&)> cb) { onItemPickedUp_ = std::move(cb); }
    void set_on_inventory_update(std::function<void(const shared::proto::InventoryUpdate&)> cb) { onInventoryUpdate_ = std::move(cb); }

    const std::optional<shared::proto::ServerHello>& server_hello() const { return serverHello_; }
    const std::optional<shared::proto::JoinAck>& join_ack() const { return joinAck_; }
    const std::optional<shared::proto::StateSnapshot>& latest_snapshot() const { return latestSnapshot_; }

    std::vector<shared::proto::BlockPlaced> take_pending_block_placed();
    std::vector<shared::proto::BlockBroken> take_pending_block_broken();
    std::vector<shared::proto::ChunkData> take_pending_chunk_data();

private:
    std::shared_ptr<shared::transport::IEndpoint> endpoint_;

    std::uint32_t inputSeq_{0};
    std::uint32_t actionSeq_{0};

    std::optional<shared::proto::ServerHello> serverHello_;
    std::optional<shared::proto::JoinAck> joinAck_;
    std::optional<shared::proto::StateSnapshot> latestSnapshot_;

    // Block event callbacks
    std::function<void(const shared::proto::BlockPlaced&)> onBlockPlaced_;
    std::function<void(const shared::proto::BlockBroken&)> onBlockBroken_;
    std::function<void(const shared::proto::ActionRejected&)> onActionRejected_;
    std::function<void(const shared::proto::ExportResult&)> onExportResult_;
    std::function<void(const shared::proto::ChunkData&)> onChunkData_;

    // Game event callbacks
    std::function<void(const shared::proto::TeamAssigned&)> onTeamAssigned_;
    std::function<void(const shared::proto::HealthUpdate&)> onHealthUpdate_;
    std::function<void(const shared::proto::PlayerDied&)> onPlayerDied_;
    std::function<void(const shared::proto::PlayerRespawned&)> onPlayerRespawned_;
    std::function<void(const shared::proto::BedDestroyed&)> onBedDestroyed_;
    std::function<void(const shared::proto::TeamEliminated&)> onTeamEliminated_;
    std::function<void(const shared::proto::MatchEnded&)> onMatchEnded_;
    std::function<void(const shared::proto::ItemSpawned&)> onItemSpawned_;
    std::function<void(const shared::proto::ItemPickedUp&)> onItemPickedUp_;
    std::function<void(const shared::proto::InventoryUpdate&)> onInventoryUpdate_;

    std::vector<shared::proto::BlockPlaced> pendingBlockPlaced_;
    std::vector<shared::proto::BlockBroken> pendingBlockBroken_;
    std::vector<shared::proto::ChunkData> pendingChunkData_;
};

} // namespace client::net
