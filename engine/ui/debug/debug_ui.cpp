// NOTE(migration): Phase 3 will replace with ImGui debug panels.
#include "debug_ui.hpp"

#include "engine/core/math_types.hpp"
#include "engine/core/logging.hpp"

namespace ui::debug {

void init() {
    // Stubbed — Phase 3 ImGui
}

static void draw_player_info(const UIViewModel& vm) {
    (void)vm;
    // Stubbed — Phase 3 ImGui
}

static void draw_net_info(const UIViewModel& vm) {
    (void)vm;
    // Stubbed — Phase 3 ImGui
}

DebugUIResult draw_interactive(const DebugUIState& current, const UIViewModel& vm) {
    (void)vm;
    DebugUIResult out;
    out.state = current;
    return out;
}

DebugUIResult draw_overlay(const DebugUIState& current, const UIViewModel& vm) {
    (void)vm;
    DebugUIResult out;
    out.state = current;
    return out;
}

} // namespace ui::debug
