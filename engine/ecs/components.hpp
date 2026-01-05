#pragma once

#include <raylib.h>
#include <entt/entt.hpp>

#include "engine/core/player_constants.hpp"

namespace ecs {

struct Transform {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f};  // Euler angles (pitch, yaw, roll)
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

struct Velocity {
    Vector3 linear{0.0f, 0.0f, 0.0f};
    Vector3 angular{0.0f, 0.0f, 0.0f};
};

struct PreviousPosition {
    Vector3 value{0.0f, 0.0f, 0.0f};
    bool initialized{false};
};

struct BoxCollider {
    Vector3 size{1.0f, 1.0f, 1.0f};
    Vector3 offset{0.0f, 0.0f, 0.0f};
    bool is_trigger{false};
};


struct PlayerTag {};

struct PlayerController {
    float move_speed{5.0f};
    float sprint_speed{8.0f};
    float jump_velocity{8.0f};
    float camera_sensitivity{0.1f};
    
    bool on_ground{false};
    bool is_sprinting{false};
    bool in_creative_mode{false};
};

struct FirstPersonCamera {
    float yaw{0.0f};
    float pitch{0.0f};
    float fov{60.0f};
    float eye_height{engine::kPlayerEyeHeight};
    float near_plane{0.1f};
    float far_plane{1000.0f};
};


struct GravityAffected {
    float gravity_scale{1.0f};
};

struct RigidBody {
    float mass{1.0f};
    float drag{0.0f};
    float angular_drag{0.05f};
    bool use_gravity{true};
    bool is_kinematic{false};
};

struct MeshComponent {
    Mesh mesh{};
    Material material{};
    bool cast_shadow{true};
};

struct ModelComponent {
    Model model{};
    bool visible{true};
};

struct ToolHolder {
    enum class ToolType {
        None,
        Pickaxe,
        Axe,
        Shovel,
        Sword
    };
    
    enum class ToolLevel {
        Hand,
        Wood,
        Stone,
        Iron,
        Diamond
    };
    
    ToolType tool_type{ToolType::None};
    ToolLevel tool_level{ToolLevel::Hand};
    
    float get_mining_speed() const;
    int get_harvest_level() const;
};

struct BlockBreaker {
    bool is_breaking{false};
    float break_progress{0.0f};
    int target_block_x{0};
    int target_block_y{0};
    int target_block_z{0};
    bool has_target{false};
};

struct InputState {
    Vector2 move_input{0.0f, 0.0f};
    Vector2 look_input{0.0f, 0.0f};
    bool jump_pressed{false};
    bool sprint_pressed{false};
    bool primary_action{false};   // Left mouse
    bool secondary_action{false}; // Right mouse
};

struct NameTag {
    const char* name{nullptr};
};

struct Lifetime {
    float remaining{0.0f};
};

struct Active {
    bool value{true};
};

} // namespace ecs
