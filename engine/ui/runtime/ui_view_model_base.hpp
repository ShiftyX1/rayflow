#pragma once

// Engine UI View Model - Base types for any game
// Game-specific view models should extend these or add their own data.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <raylib.h>

namespace ui {

// ============================================================================
// Core Game Screens (games can add their own)
// ============================================================================

enum class GameScreen {
    MainMenu,
    ConnectMenu,   // Multiplayer server address input
    Connecting,    // Connecting to server (loading state)
    Playing,
    Paused,
    Settings,
};

// ============================================================================
// Base Player View Model (engine-level)
// ============================================================================

struct PlayerViewModel {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 velocity{0.0f, 0.0f, 0.0f};

    bool on_ground{false};
    bool sprinting{false};
    bool creative{false};

    float yaw{0.0f};
    float pitch{0.0f};
    float camera_sensitivity{0.1f};

    // HUD stats (client-side view only; authoritative source is server/game state)
    int health{20};
    int max_health{20};
    
    // Team info (generic - games define team meanings)
    std::uint8_t team_id{0};
    bool can_respawn{true};
    
    // Generic resources (games can use or extend)
    std::uint16_t resource_slots[4]{0, 0, 0, 0};
};

// ============================================================================
// Network View Model (engine-level)
// ============================================================================

struct NetViewModel {
    bool has_server_hello{false};
    std::uint32_t tick_rate{0};
    std::uint32_t world_seed{0};

    bool has_join_ack{false};
    std::uint32_t player_id{0};

    bool has_snapshot{false};
    std::uint64_t server_tick{0};

    // Connection status
    bool is_connecting{false};
    bool connection_failed{false};
    std::string connection_error{};

    bool is_remote_connection{false};
    std::uint32_t ping_ms{0};
};

// ============================================================================
// Kill Feed Entry (generic)
// ============================================================================

struct KillFeedEntry {
    std::uint32_t killer_id{0};
    std::uint32_t victim_id{0};
    bool is_final_kill{false};
    float time_remaining{5.0f};
};

// ============================================================================
// Game Notification (generic)
// ============================================================================

struct GameNotification {
    std::string message;
    Color color{WHITE};
    float time_remaining{3.0f};
};

// ============================================================================
// Base Game View Model (engine-level)
// ============================================================================

struct GameViewModel {
    bool match_in_progress{false};
    bool match_ended{false};
    std::uint8_t winner_team{0};
    
    std::vector<KillFeedEntry> kill_feed;
    std::vector<GameNotification> notifications;
};

// ============================================================================
// Base UI View Model (engine-level)
// ============================================================================

struct UIViewModel {
    int screen_width{0};
    int screen_height{0};

    float dt{0.0f};
    int fps{0};

    GameScreen game_screen{GameScreen::MainMenu};

    PlayerViewModel player{};
    NetViewModel net{};
    GameViewModel game{};
};

} // namespace ui
