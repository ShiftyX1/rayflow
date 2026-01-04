#pragma once

// BedWars-specific UI View Model
// Extends engine base view model with BedWars game data.

#include "engine/ui/runtime/ui_view_model_base.hpp"
#include "../../shared/game/team_types.hpp"
#include "../../shared/game/item_types.hpp"

namespace bedwars::ui {

// Re-export base types
using ::ui::GameScreen;
using ::ui::KillFeedEntry;
using ::ui::GameNotification;
using ::ui::NetViewModel;

// ============================================================================
// BedWars Resource Count
// ============================================================================

struct ResourceCount {
    bedwars::game::ItemType type{bedwars::game::ItemType::None};
    std::uint16_t count{0};
};

// ============================================================================
// BedWars Player View Model
// ============================================================================

struct BedWarsPlayerViewModel {
    // Base player data
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 velocity{0.0f, 0.0f, 0.0f};

    bool on_ground{false};
    bool sprinting{false};
    bool creative{false};

    float yaw{0.0f};
    float pitch{0.0f};
    float camera_sensitivity{0.1f};

    int health{20};
    int max_health{20};
    
    // BedWars-specific team info
    bedwars::game::TeamId team_id{bedwars::game::Teams::None};
    bool can_respawn{true};  // False when bed is destroyed
    
    // BedWars resources
    std::uint16_t iron{0};
    std::uint16_t gold{0};
    std::uint16_t diamond{0};
    std::uint16_t emerald{0};
};

// ============================================================================
// BedWars Game View Model
// ============================================================================

struct BedWarsGameViewModel {
    bool match_in_progress{false};
    bool match_ended{false};
    bedwars::game::TeamId winner_team{bedwars::game::Teams::None};
    
    std::vector<KillFeedEntry> kill_feed;
    std::vector<GameNotification> notifications;
    
    // Team bed status (true = bed intact)
    bool team_beds[bedwars::game::Teams::MaxTeams + 1]{true, true, true, true, true, true, true, true, true};
};

// ============================================================================
// BedWars UI View Model
// ============================================================================

struct BedWarsUIViewModel {
    int screen_width{0};
    int screen_height{0};

    float dt{0.0f};
    int fps{0};

    GameScreen game_screen{GameScreen::MainMenu};

    BedWarsPlayerViewModel player{};
    NetViewModel net{};
    BedWarsGameViewModel game{};
};

} // namespace bedwars::ui
