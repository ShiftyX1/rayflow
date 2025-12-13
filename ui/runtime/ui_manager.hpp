#pragma once

#include "ui_frame.hpp"
#include "ui_view_model.hpp"

namespace ui {

class UIManager {
public:
    UIManager() = default;

    void init();

    // Update UI state for the frame (input capture + queued commands).
    UIFrameOutput update(const UIFrameInput& in, const UIViewModel& vm);

    // Render UI for the frame (must be called between BeginDrawing/EndDrawing).
    void render(const UIViewModel& vm);

private:
    void queue_command_if_changed(float prev, float next);

    bool debug_open_{false};

    bool show_player_info_{true};
    bool show_net_info_{true};

    float camera_sensitivity_{0.1f};

    std::vector<UICommand> pending_commands_{};
    UIFrameOutput last_frame_{};
};

} // namespace ui
