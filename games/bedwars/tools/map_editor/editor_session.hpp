#pragma once

// =============================================================================
// EditorSession - Map editor client using engine transport directly
// =============================================================================

#include <engine/transport/transport.hpp>
#include <games/bedwars/shared/protocol/messages.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace editor {

namespace proto = bedwars::proto;

class EditorSession {
public:
    explicit EditorSession(std::shared_ptr<engine::transport::IClientTransport> transport);

    void start_handshake(const std::string& clientName = "MapEditor");
    
    /// Poll for incoming messages. Call every frame.
    void poll();

    // --- Sending ---
    
    void send_input(float moveX, float moveY, float yaw, float pitch, 
                    bool jump, bool sprint, bool camUp = false, bool camDown = false);
    
    void send_try_set_block(int x, int y, int z, shared::voxel::BlockType blockType,
                            float hitY = 0.5f, std::uint8_t face = 2);
    
    void send_try_export_map(const std::string& mapId,
                             std::uint32_t version,
                             int chunkMinX, int chunkMinZ,
                             int chunkMaxX, int chunkMaxZ,
                             std::uint8_t skyboxKind,
                             float timeOfDayHours,
                             bool useMoon,
                             float sunIntensity,
                             float ambientIntensity,
                             float temperature,
                             float humidity);

    // --- State accessors ---
    
    const std::optional<proto::ServerHello>& server_hello() const { return serverHello_; }
    const std::optional<proto::JoinAck>& join_ack() const { return joinAck_; }
    const std::optional<proto::StateSnapshot>& latest_snapshot() const { return latestSnapshot_; }
    
    bool is_connected() const;

    // --- Callbacks ---
    
    void set_on_block_placed(std::function<void(const proto::BlockPlaced&)> cb) { onBlockPlaced_ = std::move(cb); }
    void set_on_block_broken(std::function<void(const proto::BlockBroken&)> cb) { onBlockBroken_ = std::move(cb); }
    void set_on_action_rejected(std::function<void(const proto::ActionRejected&)> cb) { onActionRejected_ = std::move(cb); }
    void set_on_export_result(std::function<void(const proto::ExportResult&)> cb) { onExportResult_ = std::move(cb); }

private:
    void handle_message(const proto::Message& msg);
    void send_message(const proto::Message& msg);

    std::shared_ptr<engine::transport::IClientTransport> transport_;

    std::uint32_t inputSeq_{0};
    std::uint32_t actionSeq_{0};

    std::optional<proto::ServerHello> serverHello_;
    std::optional<proto::JoinAck> joinAck_;
    std::optional<proto::StateSnapshot> latestSnapshot_;

    std::function<void(const proto::BlockPlaced&)> onBlockPlaced_;
    std::function<void(const proto::BlockBroken&)> onBlockBroken_;
    std::function<void(const proto::ActionRejected&)> onActionRejected_;
    std::function<void(const proto::ExportResult&)> onExportResult_;
};

} // namespace editor
