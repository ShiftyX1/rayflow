#include "fps_input_system.hpp"
#include "engine/client/core/config.hpp"
#include "engine/client/core/input.hpp"
#include "engine/core/math_types.hpp"
#include <cmath>

namespace ecs {

void FpsInputSystem::update(entt::registry& registry, float delta_time) {
    update_player_input(registry);
    update_camera_look(registry, delta_time);
}

void FpsInputSystem::update_player_input(entt::registry& registry) {
    auto view = registry.view<InputState, PlayerTag>();

    const auto& controls = core::Config::instance().controls();
    auto& input = rf::Input::instance();
    
    for (auto entity : view) {
        auto& state = view.get<InputState>(entity);
        
        state.move_input = {0.0f, 0.0f};
        if (input.isKeyDown(controls.move_forward)) state.move_input.y += 1.0f;
        if (input.isKeyDown(controls.move_backward)) state.move_input.y -= 1.0f;
        if (input.isKeyDown(controls.move_left)) state.move_input.x += 1.0f;
        if (input.isKeyDown(controls.move_right)) state.move_input.x -= 1.0f;
        
        float length = std::sqrt(state.move_input.x * state.move_input.x + 
                                  state.move_input.y * state.move_input.y);
        if (length > 0.0f) {
            state.move_input.x /= length;
            state.move_input.y /= length;
        }
        
        rf::Vec2 mouse_delta = input.getMouseDelta();
        state.look_input = {mouse_delta.x, mouse_delta.y};
        
        state.jump_pressed = input.isKeyDown(controls.jump);
        state.sprint_pressed = input.isKeyDown(controls.sprint);
        state.primary_action = input.isMouseButtonDown(controls.primary_mouse);
        state.secondary_action = input.isMouseButtonDown(controls.secondary_mouse);
    }
}

void FpsInputSystem::update_camera_look(entt::registry& registry, float delta_time) {
    auto view = registry.view<FirstPersonCamera, InputState, PlayerController>();
    
    for (auto entity : view) {
        auto& camera = view.get<FirstPersonCamera>(entity);
        auto& input = view.get<InputState>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        camera.yaw -= input.look_input.x * player.camera_sensitivity;
        camera.pitch -= input.look_input.y * player.camera_sensitivity;
        
        if (camera.pitch > 89.0f) camera.pitch = 89.0f;
        if (camera.pitch < -89.0f) camera.pitch = -89.0f;
    }
}

} // namespace ecs
