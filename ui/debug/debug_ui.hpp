#pragma once

#include "../runtime/ui_view_model.hpp"

namespace ui::debug {

struct DebugUIState {
    bool show_player_info{true};
    bool show_net_info{true};

    bool lighting_raymarch_shadows{false};

    float camera_sensitivity{0.1f};
};

struct DebugUIResult {
    DebugUIState state;
};

void init();
DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm);
DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm);

} // namespace ui::debug
