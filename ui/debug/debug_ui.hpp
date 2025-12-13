#pragma once

#include "../runtime/ui_view_model.hpp"

namespace ui::debug {

struct DebugUIState {
    bool show_player_info{true};
    bool show_net_info{true};

    float camera_sensitivity{0.1f};
};

struct DebugUIResult {
    DebugUIState state;
};

void init();
DebugUIResult draw(const DebugUIState& current, const UIViewModel& vm);

} // namespace ui::debug
