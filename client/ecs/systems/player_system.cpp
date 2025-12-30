#include "player_system.hpp"
#include "../../voxel/world.hpp"
#include "../../core/config.hpp"
#include "../../../shared/constants.hpp"
#include <raylib.h>
#include <cmath>
#include <cstdio>

namespace ecs {

constexpr float DEG_TO_RAD = 0.017453292519943295f;

void PlayerSystem::update(entt::registry& registry, float delta_time) {
    handle_tool_selection(registry);

    if (clientReplicaMode_) {
        return;
    }

    handle_creative_mode(registry);
    handle_movement(registry, delta_time);
    handle_jumping(registry);
}

entt::entity PlayerSystem::create_player(entt::registry& registry, const Vector3& spawn_position) {
    auto entity = registry.create();
    
    registry.emplace<PlayerTag>(entity);
    registry.emplace<NameTag>(entity, "Player");
    
    auto& transform = registry.emplace<Transform>(entity);
    transform.position = spawn_position;
    
    registry.emplace<Velocity>(entity);
    registry.emplace<GravityAffected>(entity);
    
    auto& collider = registry.emplace<BoxCollider>(entity);
    collider.size = {shared::kPlayerWidth, shared::kPlayerHeight, shared::kPlayerWidth};
    
    auto& controller = registry.emplace<PlayerController>(entity);
    controller.move_speed = 5.0f;
    controller.sprint_speed = 8.0f;
    controller.jump_velocity = 8.0f;
    controller.camera_sensitivity = 0.1f;
    
    auto& camera = registry.emplace<FirstPersonCamera>(entity);
    camera.eye_height = shared::kPlayerEyeHeight;
    camera.fov = 60.0f;
    
    registry.emplace<InputState>(entity);
    
    registry.emplace<ToolHolder>(entity);
    registry.emplace<BlockBreaker>(entity);
    
    return entity;
}

Camera3D PlayerSystem::get_camera(entt::registry& registry, entt::entity player) {
    auto& transform = registry.get<Transform>(player);
    auto& fps_camera = registry.get<FirstPersonCamera>(player);
    
    Camera3D camera = {};
    camera.position = {
        transform.position.x,
        transform.position.y + fps_camera.eye_height,
        transform.position.z
    };
    
    float yaw_rad = fps_camera.yaw * DEG_TO_RAD;
    float pitch_rad = fps_camera.pitch * DEG_TO_RAD;
    
    Vector3 direction;
    direction.x = std::cos(pitch_rad) * std::sin(yaw_rad);
    direction.y = std::sin(pitch_rad);
    direction.z = std::cos(pitch_rad) * std::cos(yaw_rad);
    
    camera.target = {
        camera.position.x + direction.x,
        camera.position.y + direction.y,
        camera.position.z + direction.z
    };
    
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = fps_camera.fov;
    camera.projection = CAMERA_PERSPECTIVE;
    
    return camera;
}

void PlayerSystem::handle_movement(entt::registry& registry, float delta_time) {
    auto view = registry.view<Transform, Velocity, InputState, PlayerController, FirstPersonCamera>();

    const auto& controls = core::Config::instance().controls();
    
    for (auto entity : view) {
        auto& velocity = view.get<Velocity>(entity);
        auto& input = view.get<InputState>(entity);
        auto& player = view.get<PlayerController>(entity);
        auto& camera = view.get<FirstPersonCamera>(entity);
        
        player.is_sprinting = input.sprint_pressed && input.move_input.y > 0;
        float speed = player.is_sprinting ? player.sprint_speed : player.move_speed;
        
        float yaw_rad = camera.yaw * DEG_TO_RAD;
        
        Vector3 forward = {std::sin(yaw_rad), 0.0f, std::cos(yaw_rad)};
        Vector3 right = {std::cos(yaw_rad), 0.0f, -std::sin(yaw_rad)};
        
        float move_x = input.move_input.x * speed;
        float move_z = input.move_input.y * speed;
        
        velocity.linear.x = right.x * move_x + forward.x * move_z;
        velocity.linear.z = right.z * move_x + forward.z * move_z;
        
        if (player.in_creative_mode) {
            velocity.linear.y = 0.0f;
            if (input.jump_pressed) {
                velocity.linear.y = speed;
            }
            if (IsKeyDown(controls.fly_down)) {
                velocity.linear.y = -speed;
            }
        }
    }
}

void PlayerSystem::handle_jumping(entt::registry& registry) {
    auto view = registry.view<Velocity, InputState, PlayerController>();
    
    for (auto entity : view) {
        auto& velocity = view.get<Velocity>(entity);
        auto& input = view.get<InputState>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        if (!player.in_creative_mode && player.on_ground && input.jump_pressed) {
            velocity.linear.y = player.jump_velocity;
            player.on_ground = false;
        }
    }
}

void PlayerSystem::handle_creative_mode(entt::registry& registry) {
    auto view = registry.view<PlayerController>();

    const auto& controls = core::Config::instance().controls();
    
    if (IsKeyPressed(controls.toggle_creative)) {
        for (auto entity : view) {
            auto& player = view.get<PlayerController>(entity);
            player.in_creative_mode = !player.in_creative_mode;
            TraceLog(LOG_INFO, "Creative mode: %s", player.in_creative_mode ? "ON" : "OFF");
        }
    }
}

void PlayerSystem::handle_tool_selection(entt::registry& registry) {
    auto view = registry.view<ToolHolder>();

    const auto& controls = core::Config::instance().controls();
    
    for (auto entity : view) {
        auto& tool = view.get<ToolHolder>(entity);
        
        if (IsKeyPressed(controls.tool_1)) {
            tool.tool_type = ToolHolder::ToolType::None;
            tool.tool_level = ToolHolder::ToolLevel::Hand;
            TraceLog(LOG_INFO, "Selected: Hand");
        }
        if (IsKeyPressed(controls.tool_2)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Wood;
            TraceLog(LOG_INFO, "Selected: Wooden Pickaxe");
        }
        if (IsKeyPressed(controls.tool_3)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Stone;
            TraceLog(LOG_INFO, "Selected: Stone Pickaxe");
        }
        if (IsKeyPressed(controls.tool_4)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Iron;
            TraceLog(LOG_INFO, "Selected: Iron Pickaxe");
        }
        if (IsKeyPressed(controls.tool_5)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Diamond;
            TraceLog(LOG_INFO, "Selected: Diamond Pickaxe");
        }
    }
}

} // namespace ecs
