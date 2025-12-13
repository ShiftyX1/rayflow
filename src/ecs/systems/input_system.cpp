#include "input_system.hpp"
#include <raylib.h>
#include <cmath>

namespace ecs {

void InputSystem::update(entt::registry& registry, float delta_time) {
    update_player_input(registry);
    update_camera_look(registry, delta_time);
}

void InputSystem::update_player_input(entt::registry& registry) {
    auto view = registry.view<InputState, PlayerTag>();
    
    for (auto entity : view) {
        auto& input = view.get<InputState>(entity);
        
        // Movement input (WASD)
        input.move_input = {0.0f, 0.0f};
        if (IsKeyDown(KEY_W)) input.move_input.y += 1.0f;
        if (IsKeyDown(KEY_S)) input.move_input.y -= 1.0f;
        if (IsKeyDown(KEY_A)) input.move_input.x += 1.0f;
        if (IsKeyDown(KEY_D)) input.move_input.x -= 1.0f;
        
        // Normalize movement input
        float length = std::sqrt(input.move_input.x * input.move_input.x + 
                                  input.move_input.y * input.move_input.y);
        if (length > 0.0f) {
            input.move_input.x /= length;
            input.move_input.y /= length;
        }
        
        // Look input (mouse delta)
        Vector2 mouse_delta = GetMouseDelta();
        input.look_input = {mouse_delta.x, mouse_delta.y};
        
        // Action buttons
        input.jump_pressed = IsKeyDown(KEY_SPACE);
        input.sprint_pressed = IsKeyDown(KEY_LEFT_CONTROL);
        input.primary_action = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        input.secondary_action = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
    }
}

void InputSystem::update_camera_look(entt::registry& registry, float delta_time) {
    auto view = registry.view<FirstPersonCamera, InputState, PlayerController>();
    
    for (auto entity : view) {
        auto& camera = view.get<FirstPersonCamera>(entity);
        auto& input = view.get<InputState>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        // Apply mouse movement to camera rotation
        camera.yaw -= input.look_input.x * player.camera_sensitivity;
        camera.pitch -= input.look_input.y * player.camera_sensitivity;
        
        // Clamp pitch to prevent camera flip
        if (camera.pitch > 89.0f) camera.pitch = 89.0f;
        if (camera.pitch < -89.0f) camera.pitch = -89.0f;
    }
}

} // namespace ecs
