#pragma once

#include <cstdint>
#include <optional>

#include <raylib.h>

namespace ui {

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
};

struct NetViewModel {
    bool has_server_hello{false};
    std::uint32_t tick_rate{0};
    std::uint32_t world_seed{0};

    bool has_join_ack{false};
    std::uint32_t player_id{0};

    bool has_snapshot{false};
    std::uint64_t server_tick{0};
};

struct UIViewModel {
    int screen_width{0};
    int screen_height{0};

    float dt{0.0f};
    int fps{0};

    PlayerViewModel player{};
    NetViewModel net{};
};

} // namespace ui
