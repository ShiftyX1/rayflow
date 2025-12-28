#include "ui_manager.hpp"

#include <raylib.h>

#ifdef DEBUG_UI
#include "../debug/debug_ui.hpp"
#endif

namespace ui {

UIManager::~UIManager() {
    main_menu_.unload();
    connect_menu_.unload();
    pause_menu_.unload();
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

    // Load connect menu UI
    connect_menu_.set_on_click([this](const std::string& action) {
        handle_ui_click(action);
    });
    connect_menu_loaded_ = connect_menu_.load_from_files("ui/connect_menu.xml", "ui/connect_menu.css");
    TraceLog(LOG_INFO, "[ui] Connect menu loaded: %s", connect_menu_loaded_ ? "true" : "false");

    // Load pause menu UI
    pause_menu_.set_on_click([this](const std::string& action) {
        handle_ui_click(action);
    });
    pause_menu_loaded_ = pause_menu_.load_from_files("ui/pause_menu.xml", "ui/pause_menu.css");
    TraceLog(LOG_INFO, "[ui] Pause menu loaded: %s", pause_menu_loaded_ ? "true" : "false");

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
    } else if (action == "return_to_main_menu") {
        pending_commands_.emplace_back(ReturnToMainMenu{});
    } else if (action == "show_connect_screen") {
        pending_commands_.emplace_back(ShowConnectScreen{});
    } else if (action == "hide_connect_screen") {
        pending_commands_.emplace_back(HideConnectScreen{});
    } else if (action == "connect_to_server") {
        // Parse server address
        std::string host = "localhost";
        std::uint16_t port = 7777;
        
        auto colon = server_address_.find(':');
        if (colon != std::string::npos) {
            host = server_address_.substr(0, colon);
            try {
                port = static_cast<std::uint16_t>(std::stoi(server_address_.substr(colon + 1)));
            } catch (...) {
                port = 7777;
            }
        } else {
            host = server_address_;
        }
        
        pending_commands_.emplace_back(ConnectToServer{host, port});
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
    } else if (vm.game_screen == GameScreen::ConnectMenu) {
        // Connect menu captures input
        out.capture.wants_mouse = true;
        out.capture.wants_keyboard = true;

        // Handle text input for server address
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && server_address_.length() < 64) {
                server_address_ += static_cast<char>(key);
            }
            key = GetCharPressed();
        }
        
        if (IsKeyPressed(KEY_BACKSPACE) && !server_address_.empty()) {
            server_address_.pop_back();
        }
        
        // ESC to go back
        if (in.toggle_pause) {
            pending_commands_.emplace_back(HideConnectScreen{});
        }
        
        // Enter to connect
        if (IsKeyPressed(KEY_ENTER)) {
            handle_ui_click("connect_to_server");
        }

        if (connect_menu_loaded_) {
            connect_menu_.update(vm, mouse_pos, mouse_down, mouse_pressed);
        }
    } else if (vm.game_screen == GameScreen::Connecting) {
        // Connecting screen - just show status
        out.capture.wants_mouse = true;
        out.capture.wants_keyboard = true;
        
        // ESC to cancel
        if (in.toggle_pause) {
            pending_commands_.emplace_back(DisconnectFromServer{});
        }
    } else if (vm.game_screen == GameScreen::Paused) {
        // Pause menu captures input
        out.capture.wants_mouse = true;
        out.capture.wants_keyboard = true;

        // Handle ESC to resume game
        if (in.toggle_pause) {
            pending_commands_.emplace_back(ResumeGame{});
        }

        if (pause_menu_loaded_) {
            pause_menu_.update(vm, mouse_pos, mouse_down, mouse_pressed);
        }
    } else if (vm.game_screen == GameScreen::Playing) {
        // Handle ESC to open pause menu
        if (in.toggle_pause) {
            pending_commands_.emplace_back(OpenPauseMenu{});
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
        DrawRectangle(0, 0, vm.screen_width, vm.screen_height, Color{1, 4, 9, 255});
        
        if (main_menu_loaded_) {
            main_menu_.render(vm);
        }
    } else if (vm.game_screen == GameScreen::ConnectMenu) {
        DrawRectangle(0, 0, vm.screen_width, vm.screen_height, Color{1, 4, 9, 255});
        
        if (connect_menu_loaded_) {
            connect_menu_.render(vm);
        }
        
        // Draw text input content over the hint
        // Calculate input box position (centered, below title)
        const int inputX = (vm.screen_width - 340) / 2;
        const int inputY = (vm.screen_height - 380) / 2 + 160;
        
        // Draw address text
        DrawText(server_address_.c_str(), inputX + 12, inputY + 14, 16, Color{201, 209, 217, 255});
        
        // Draw cursor
        const int cursorX = inputX + 12 + MeasureText(server_address_.c_str(), 16);
        if ((static_cast<int>(GetTime() * 2) % 2) == 0) {
            DrawRectangle(cursorX, inputY + 12, 2, 20, Color{88, 166, 255, 255});
        }
    } else if (vm.game_screen == GameScreen::Connecting) {
        DrawRectangle(0, 0, vm.screen_width, vm.screen_height, Color{1, 4, 9, 255});
        
        // Draw connecting message
        const char* text = "Connecting...";
        const int textW = MeasureText(text, 32);
        DrawText(text, (vm.screen_width - textW) / 2, vm.screen_height / 2 - 40, 32, Color{88, 166, 255, 255});
        
        // Draw cancel hint
        const char* hint = "Press ESC to cancel";
        const int hintW = MeasureText(hint, 16);
        DrawText(hint, (vm.screen_width - hintW) / 2, vm.screen_height / 2 + 20, 16, Color{139, 148, 158, 255});
        
        // Show error if any
        if (vm.net.connection_failed && !vm.net.connection_error.empty()) {
            const int errW = MeasureText(vm.net.connection_error.c_str(), 18);
            DrawText(vm.net.connection_error.c_str(), (vm.screen_width - errW) / 2, vm.screen_height / 2 + 60, 18, Color{248, 81, 73, 255});
        }
    } else if (vm.game_screen == GameScreen::Paused) {
        // Note: world is still rendered in game.cpp, we just overlay pause menu
        if (pause_menu_loaded_) {
            pause_menu_.render(vm);
        }
    } else if (vm.game_screen == GameScreen::Playing) {
        if (hud_loaded_) {
            hud_.render(vm);
        }
    }
}

} // namespace ui
