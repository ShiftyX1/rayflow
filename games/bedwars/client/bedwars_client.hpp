#pragma once

// BedWars Client - IGameClient implementation
// Renders voxel world, handles input, processes server messages.
// Uses engine UI system for menus.

#include "engine/core/game_interface.hpp"
#include "engine/core/types.hpp"
#include "engine/ui/runtime/ui_view_model.hpp"
#include "engine/ui/runtime/ui_frame.hpp"

#include "../shared/protocol/messages.hpp"

#include <entt/entt.hpp>
#include <raylib.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for ECS systems
namespace ecs {
    class InputSystem;
    class PlayerSystem;
}

namespace bedwars {

// ============================================================================
// Connection Callbacks (set by client_main)
// ============================================================================

/// Callback to start singleplayer mode (creates embedded server)
using StartSingleplayerCallback = std::function<void()>;

/// Callback to connect to multiplayer server
using ConnectMultiplayerCallback = std::function<bool(const std::string& host, std::uint16_t port)>;

/// Callback to disconnect from server
using DisconnectCallback = std::function<void()>;

// ============================================================================
// Game State
// ============================================================================

/// Player state on client side (replicated from server)
struct ClientPlayerState {
    std::uint32_t playerId{0};
    std::string name;
    proto::TeamId team{proto::Teams::None};
    
    // Position (current, interpolated each frame)
    float px{0.0f}, py{80.0f}, pz{0.0f};
    float vx{0.0f}, vy{0.0f}, vz{0.0f};
    
    // Target position from server (updated on snapshot)
    float targetPx{0.0f}, targetPy{80.0f}, targetPz{0.0f};
    
    // Camera
    float yaw{0.0f};
    float pitch{0.0f};
    
    // Combat
    std::uint8_t hp{20};
    std::uint8_t maxHp{20};
    bool alive{true};
};

/// Team state on client
struct ClientTeamState {
    proto::TeamId id{proto::Teams::None};
    bool hasBed{true};
    Color color{WHITE};
};

/// Dropped item on client
struct ClientItemState {
    std::uint32_t entityId{0};
    proto::ItemType type;
    float x{0.0f}, y{0.0f}, z{0.0f};
    std::uint16_t count{1};
};

// ============================================================================
// BedWarsClient - IGameClient implementation
// ============================================================================

class BedWarsClient : public engine::IGameClient {
public:
    BedWarsClient();
    ~BedWarsClient() override;

    // --- Configuration ---
    
    void set_player_name(const std::string& name) { playerName_ = name; }
    
    // --- Connection callbacks (set by client_main) ---
    
    void set_start_singleplayer_callback(StartSingleplayerCallback cb) { onStartSingleplayer_ = std::move(cb); }
    void set_connect_multiplayer_callback(ConnectMultiplayerCallback cb) { onConnectMultiplayer_ = std::move(cb); }
    void set_disconnect_callback(DisconnectCallback cb) { onDisconnect_ = std::move(cb); }

    // --- IGameClient implementation ---
    
    void on_init(engine::IClientServices& engine) override;
    void on_shutdown() override;
    void on_update(float dt) override;
    void on_render() override;
    void on_connected() override;
    void on_disconnected() override;
    void on_server_message(std::span<const std::uint8_t> data) override;

private:
    // --- Message handlers ---
    void handle_message(const proto::Message& msg);
    void handle_server_hello(const proto::ServerHello& msg);
    void handle_join_ack(const proto::JoinAck& msg);
    void handle_state_snapshot(const proto::StateSnapshot& msg);
    void handle_chunk_data(const proto::ChunkData& msg);
    void handle_block_placed(const proto::BlockPlaced& msg);
    void handle_block_broken(const proto::BlockBroken& msg);
    void handle_action_rejected(const proto::ActionRejected& msg);
    void handle_team_assigned(const proto::TeamAssigned& msg);
    void handle_health_update(const proto::HealthUpdate& msg);
    void handle_player_died(const proto::PlayerDied& msg);
    void handle_player_respawned(const proto::PlayerRespawned& msg);
    void handle_bed_destroyed(const proto::BedDestroyed& msg);
    void handle_item_spawned(const proto::ItemSpawned& msg);
    void handle_item_picked_up(const proto::ItemPickedUp& msg);

    // --- Sending ---
    void send_client_hello();
    void send_join_match();
    void send_input_frame();
    void send_try_break_block(int x, int y, int z);
    void send_try_place_block(int x, int y, int z, proto::BlockType type, float hitY, std::uint8_t face);
    
    template<typename T>
    void send_message(const T& msg);

    // --- Input ---
    void clear_player_input();
    
    // --- Rendering ---
    void render_world(const Camera3D& camera);
    void render_players();
    void render_items();
    void render_hud();
    void render_debug_info();
    
    // --- UI ---
    void apply_ui_commands(const ui::UIFrameOutput& out);
    void update_ui_view_model();

    // --- Helpers ---
    Color get_team_color(proto::TeamId team) const;

private:
    engine::IClientServices* engine_{nullptr};
    
    // Connection callbacks
    StartSingleplayerCallback onStartSingleplayer_;
    ConnectMultiplayerCallback onConnectMultiplayer_;
    DisconnectCallback onDisconnect_;
    
    // Player config
    std::string playerName_{"Player"};
    
    // UI state
    ui::GameScreen gameScreen_{ui::GameScreen::MainMenu};
    ui::UIViewModel uiViewModel_;
    std::string connectionError_;
    
    // Session state (network)
    enum class SessionState {
        Disconnected,
        Connecting,
        WaitingServerHello,
        WaitingJoinAck,
        InGame
    };
    SessionState sessionState_{SessionState::Disconnected};
    
    // Protocol state
    std::uint32_t inputSeq_{0};
    std::uint32_t actionSeq_{0};
    
    // Server info
    std::uint32_t tickRate_{30};
    std::uint32_t worldSeed_{0};
    std::uint32_t serverTick_{0};
    bool hasMapTemplate_{false};
    std::string mapId_;
    std::uint32_t mapVersion_{0};
    
    // Local player
    std::uint32_t localPlayerId_{0};
    ClientPlayerState localPlayer_;
    
    // Other players
    std::unordered_map<std::uint32_t, ClientPlayerState> players_;
    
    // Teams
    std::array<ClientTeamState, 4> teams_;
    
    // Items in world
    std::unordered_map<std::uint32_t, ClientItemState> items_;
    
    // ECS (same as rayflow)
    entt::registry registry_;
    entt::entity playerEntity_{entt::null};
    std::unique_ptr<ecs::InputSystem> inputSystem_;
    std::unique_ptr<ecs::PlayerSystem> playerSystem_;
    
    // UI input capture
    bool uiCapturesInput_{false};
    
    // Debug
    bool showDebug_{false};
};

} // namespace bedwars
