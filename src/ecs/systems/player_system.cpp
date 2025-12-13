#include "player_system.hpp"
#include "../../voxel/world.hpp"
#include <raylib.h>
#include <cmath>
#include <cstdio>

namespace ecs {

constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_WIDTH = 0.6f;
constexpr float PLAYER_EYE_HEIGHT = 1.62f;
constexpr float DEG_TO_RAD = 0.017453292519943295f;

void PlayerSystem::update(entt::registry& registry, float delta_time) {
    handle_tool_selection(registry);
    handle_creative_mode(registry);
    handle_movement(registry, delta_time);
    handle_jumping(registry);
}

entt::entity PlayerSystem::create_player(entt::registry& registry, const Vector3& spawn_position) {
    auto entity = registry.create();
    
    // Core components
    registry.emplace<PlayerTag>(entity);
    registry.emplace<NameTag>(entity, "Player");
    
    // Transform
    auto& transform = registry.emplace<Transform>(entity);
    transform.position = spawn_position;
    
    // Physics
    registry.emplace<Velocity>(entity);
    registry.emplace<GravityAffected>(entity);
    
    auto& collider = registry.emplace<BoxCollider>(entity);
    collider.size = {PLAYER_WIDTH, PLAYER_HEIGHT, PLAYER_WIDTH};
    
    // Player controller
    auto& controller = registry.emplace<PlayerController>(entity);
    controller.move_speed = 5.0f;
    controller.sprint_speed = 8.0f;
    controller.jump_velocity = 8.0f;
    controller.camera_sensitivity = 0.1f;
    
    // Camera
    auto& camera = registry.emplace<FirstPersonCamera>(entity);
    camera.eye_height = PLAYER_EYE_HEIGHT;
    camera.fov = 60.0f;
    
    // Input
    registry.emplace<InputState>(entity);
    
    // Tool
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
    
    // Calculate camera target based on yaw and pitch
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
    
    for (auto entity : view) {
        auto& velocity = view.get<Velocity>(entity);
        auto& input = view.get<InputState>(entity);
        auto& player = view.get<PlayerController>(entity);
        auto& camera = view.get<FirstPersonCamera>(entity);
        
        // Update sprint state
        player.is_sprinting = input.sprint_pressed && input.move_input.y > 0;
        float speed = player.is_sprinting ? player.sprint_speed : player.move_speed;
        
        // Calculate movement direction based on camera yaw
        float yaw_rad = camera.yaw * DEG_TO_RAD;
        
        Vector3 forward = {std::sin(yaw_rad), 0.0f, std::cos(yaw_rad)};
        Vector3 right = {std::cos(yaw_rad), 0.0f, -std::sin(yaw_rad)};
        
        // Apply horizontal movement
        float move_x = input.move_input.x * speed;
        float move_z = input.move_input.y * speed;
        
        velocity.linear.x = right.x * move_x + forward.x * move_z;
        velocity.linear.z = right.z * move_x + forward.z * move_z;
        
        // Creative mode vertical movement
        if (player.in_creative_mode) {
            velocity.linear.y = 0.0f;
            if (input.jump_pressed) {
                velocity.linear.y = speed;
            }
            if (IsKeyDown(KEY_LEFT_SHIFT)) {
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
        
        // Only jump in normal mode when on ground
        if (!player.in_creative_mode && player.on_ground && input.jump_pressed) {
            velocity.linear.y = player.jump_velocity;
            player.on_ground = false;
        }
    }
}

void PlayerSystem::handle_creative_mode(entt::registry& registry) {
    auto view = registry.view<PlayerController>();
    
    if (IsKeyPressed(KEY_C)) {
        for (auto entity : view) {
            auto& player = view.get<PlayerController>(entity);
            player.in_creative_mode = !player.in_creative_mode;
            std::printf("Creative mode: %s\n", player.in_creative_mode ? "ON" : "OFF");
        }
    }
}

void PlayerSystem::handle_tool_selection(entt::registry& registry) {
    auto view = registry.view<ToolHolder>();
    
    for (auto entity : view) {
        auto& tool = view.get<ToolHolder>(entity);
        
        if (IsKeyPressed(KEY_ONE)) {
            tool.tool_type = ToolHolder::ToolType::None;
            tool.tool_level = ToolHolder::ToolLevel::Hand;
            std::printf("Selected: Hand\n");
        }
        if (IsKeyPressed(KEY_TWO)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Wood;
            std::printf("Selected: Wooden Pickaxe\n");
        }
        if (IsKeyPressed(KEY_THREE)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Stone;
            std::printf("Selected: Stone Pickaxe\n");
        }
        if (IsKeyPressed(KEY_FOUR)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Iron;
            std::printf("Selected: Iron Pickaxe\n");
        }
        if (IsKeyPressed(KEY_FIVE)) {
            tool.tool_type = ToolHolder::ToolType::Pickaxe;
            tool.tool_level = ToolHolder::ToolLevel::Diamond;
            std::printf("Selected: Diamond Pickaxe\n");
        }
    }
}

} // namespace ecs
