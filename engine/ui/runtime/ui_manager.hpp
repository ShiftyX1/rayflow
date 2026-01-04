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
    void handle_ui_click(const std::string& action);

    enum class DebugMode {
        Off,
        Overlay,
        Interactive,
    };

    DebugMode debug_mode_{DebugMode::Off};

    bool show_player_info_{true};
    bool show_net_info_{true};

    float camera_sensitivity_{0.1f};

    std::vector<UICommand> pending_commands_{};
    UIFrameOutput last_frame_{};

    // Main menu UI (XML + CSS-lite)
    xmlui::UIDocument main_menu_{};
    bool main_menu_loaded_{false};

    // Connect menu UI (XML + CSS-lite)
    xmlui::UIDocument connect_menu_{};
    bool connect_menu_loaded_{false};

    // Pause menu UI (XML + CSS-lite)
    xmlui::UIDocument pause_menu_{};
    bool pause_menu_loaded_{false};

    // Player HUD (XML + CSS-lite)
    xmlui::UIDocument hud_{};
    bool hud_loaded_{false};

    // Server address text input state
    std::string server_address_{"localhost:7777"};

    // Cached view model (for layout)
    UIViewModel cached_vm_{};
};

} // namespace ui
