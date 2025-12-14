#pragma once

#include "ui_frame.hpp"
#include "ui_view_model.hpp"

#include "xmlui/ui_document.hpp"

namespace ui {

class UIManager {
public:
    UIManager() = default;
    ~UIManager();

    void init();

    // Update UI state for the frame (input capture + queued commands).
    UIFrameOutput update(const UIFrameInput& in, const UIViewModel& vm);

    // Render UI for the frame (must be called between BeginDrawing/EndDrawing).
    void render(const UIViewModel& vm);

private:
    void queue_command_if_changed(float prev, float next);
    void queue_command_if_changed(bool prev, bool next);

    enum class DebugMode {
        Off,
        Overlay,
        Interactive,
    };

    DebugMode debug_mode_{DebugMode::Off};

    bool show_player_info_{true};
    bool show_net_info_{true};

    bool lighting_raymarch_shadows_{false};

    float camera_sensitivity_{0.1f};

    std::vector<UICommand> pending_commands_{};
    UIFrameOutput last_frame_{};

    // Player HUD (XML + CSS-lite)
    xmlui::UIDocument hud_{};
    bool hud_loaded_{false};
};

} // namespace ui
