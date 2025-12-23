#include "ui_manager.hpp"

#include <raylib.h>

#ifdef DEBUG_UI
#include "../debug/debug_ui.hpp"
#endif

namespace ui {

UIManager::~UIManager() {
    main_menu_.unload();
    hud_.unload();
}

void UIManager::init() {
#ifdef DEBUG_UI
    debug::init();
#endif

    // Load main menu UI
    main_menu_.set_on_click([this](const std::string& action) {
        handle_ui_click(action);
    });
    main_menu_loaded_ = main_menu_.load_from_files("ui/main_menu.xml", "ui/main_menu.css");
    TraceLog(LOG_INFO, "[ui] Main menu loaded: %s", main_menu_loaded_ ? "true" : "false");

    // Load HUD
    hud_loaded_ = hud_.load_from_files("ui/hud.xml", "ui/hud.css");
    TraceLog(LOG_INFO, "[ui] HUD loaded: %s", hud_loaded_ ? "true" : "false");
}

void UIManager::handle_ui_click(const std::string& action) {
    if (action == "start_game") {
        pending_commands_.emplace_back(StartGame{});
    } else if (action == "quit_game") {
        pending_commands_.emplace_back(QuitGame{});
    } else if (action == "open_settings") {
        pending_commands_.emplace_back(OpenSettings{});
    } else if (action == "close_settings") {
        pending_commands_.emplace_back(CloseSettings{});
    } else if (action == "resume_game") {
        pending_commands_.emplace_back(ResumeGame{});
    } else {
        // Generic button clicked
        pending_commands_.emplace_back(ButtonClicked{action});
    }
}

UIFrameOutput UIManager::update(const UIFrameInput& in, const UIViewModel& vm) {
    UIFrameOutput out;
    cached_vm_ = vm;

    if (in.toggle_debug_ui) {
        debug_mode_ = (debug_mode_ == DebugMode::Interactive) ? DebugMode::Off : DebugMode::Interactive;
    }
    if (in.toggle_debug_overlay) {
        debug_mode_ = (debug_mode_ == DebugMode::Overlay) ? DebugMode::Off : DebugMode::Overlay;
    }

#ifdef DEBUG_UI
    if (debug_mode_ == DebugMode::Interactive) {
        out.capture.wants_mouse = true;
        out.capture.wants_keyboard = true;
    }
#else
    (void)in;
#endif

    const Vector2 mouse_pos = GetMousePosition();
    const bool mouse_down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const bool mouse_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (vm.game_screen == GameScreen::MainMenu) {
        // Main menu always captures input (cursor should be visible)
        out.capture.wants_mouse = true;
        out.capture.wants_keyboard = true;

        if (main_menu_loaded_) {
            main_menu_.update(vm, mouse_pos, mouse_down, mouse_pressed);
        }
    }

    // Drain commands produced during the previous render() call.
    out.commands = std::move(pending_commands_);
    pending_commands_.clear();

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
    if (debug_mode_ == DebugMode::Interactive) {
        debug::DebugUIState state;
        state.show_player_info = show_player_info_;
        state.show_net_info = show_net_info_;
        state.camera_sensitivity = camera_sensitivity_;

        debug::DebugUIResult result = debug::draw_interactive(state, vm);

        show_player_info_ = result.state.show_player_info;
        show_net_info_ = result.state.show_net_info;

        const float prev_sens = camera_sensitivity_;
        camera_sensitivity_ = result.state.camera_sensitivity;
        queue_command_if_changed(prev_sens, camera_sensitivity_);
        return;
    }

    if (debug_mode_ == DebugMode::Overlay) {
        debug::DebugUIState state;
        state.show_player_info = true;
        state.show_net_info = true;
        state.camera_sensitivity = camera_sensitivity_;
        (void)debug::draw_overlay(state, vm);
        return;
    }
#endif

    if (vm.game_screen == GameScreen::MainMenu) {
        DrawRectangle(0, 0, vm.screen_width, vm.screen_height, Color{10, 10, 20, 255});
        
        if (main_menu_loaded_) {
            main_menu_.render(vm);
        }
    } else if (vm.game_screen == GameScreen::Playing) {
        if (hud_loaded_) {
            hud_.render(vm);
        }
    }
}

} // namespace ui
