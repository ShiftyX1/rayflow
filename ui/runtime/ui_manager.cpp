#include "ui_manager.hpp"

#include <raylib.h>

#ifdef DEBUG_UI
#include "../debug/debug_ui.hpp"
#endif

namespace ui {

void UIManager::init() {
#ifdef DEBUG_UI
    debug::init();
#endif
}

UIFrameOutput UIManager::update(const UIFrameInput& in, const UIViewModel& vm) {
    UIFrameOutput out;

    if (in.toggle_debug_ui) {
        debug_open_ = !debug_open_;
    }

#ifdef DEBUG_UI
    if (debug_open_) {
        out.capture.wants_mouse = true;
        out.capture.wants_keyboard = true;
    }
#else
    (void)in;
#endif

    // Drain commands produced during the previous render() call.
    out.commands = std::move(pending_commands_);
    pending_commands_.clear();

    (void)vm;

    last_frame_ = out;
    return out;
}

void UIManager::queue_command_if_changed(float prev, float next) {
    if (prev != next) {
        pending_commands_.emplace_back(SetCameraSensitivity{next});
    }
}

void UIManager::render(const UIViewModel& vm) {
#ifdef DEBUG_UI
    if (debug_open_) {
        debug::DebugUIState state;
        state.show_player_info = show_player_info_;
        state.show_net_info = show_net_info_;
        state.camera_sensitivity = camera_sensitivity_;

        debug::DebugUIResult result = debug::draw(state, vm);

        show_player_info_ = result.state.show_player_info;
        show_net_info_ = result.state.show_net_info;

        const float prev_sens = camera_sensitivity_;
        camera_sensitivity_ = result.state.camera_sensitivity;
        queue_command_if_changed(prev_sens, camera_sensitivity_);
        return;
    }
#endif

    // Minimal HUD (always-on) can live here later.
    (void)vm;
}

} // namespace ui
