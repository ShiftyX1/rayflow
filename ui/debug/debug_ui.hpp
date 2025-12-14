#pragma once

#include "../runtime/ui_view_model.hpp"

namespace ui::debug {

struct DebugUIState {
    bool show_player_info{true};
    bool show_net_info{true};

    bool lighting_raymarch_shadows{false};

    // Global light config (debug-only)
    float lighting_time_of_day_hours{12.0f};
    bool lighting_use_moon{false};
    float lighting_sun_intensity{1.0f};
    float lighting_ambient_intensity{0.35f};

    float camera_sensitivity{0.1f};
};

struct DebugUIResult {
    DebugUIState state;
};

void init();
DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm);
DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm);

} // namespace ui::debug
