#pragma once

#include <raylib.h>
#include <entt/entt.hpp>

#include "../../shared/constants.hpp"

namespace ecs {

// ============================================================================
// Core Components
// ============================================================================

// Transform component - position, rotation, scale in world space
struct Transform {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f};  // Euler angles (pitch, yaw, roll)
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

// Velocity component - for physics simulation
struct Velocity {
    Vector3 linear{0.0f, 0.0f, 0.0f};
    Vector3 angular{0.0f, 0.0f, 0.0f};
};

// Previous position (used for stable collision resolution)
struct PreviousPosition {
    Vector3 value{0.0f, 0.0f, 0.0f};
    bool initialized{false};
};

// AABB collision box
struct BoxCollider {
    Vector3 size{1.0f, 1.0f, 1.0f};
    Vector3 offset{0.0f, 0.0f, 0.0f};
    bool is_trigger{false};
};

// ============================================================================
// Player Components
// ============================================================================

// Tag component to identify player entity
struct PlayerTag {};

// Player-specific data
struct PlayerController {
    float move_speed{5.0f};
    float sprint_speed{8.0f};
    float jump_velocity{8.0f};
    float camera_sensitivity{0.1f};
    
    bool on_ground{false};
    bool is_sprinting{false};
    bool in_creative_mode{false};
};

// First-person camera component
struct FirstPersonCamera {
    float yaw{0.0f};
    float pitch{0.0f};
    float fov{60.0f};
    float eye_height{shared::kPlayerEyeHeight};
    float near_plane{0.1f};
    float far_plane{1000.0f};
};

// ============================================================================
// Physics Components
// ============================================================================

// Gravity affected component
struct GravityAffected {
    float gravity_scale{1.0f};
};

// Rigid body for physics simulation
struct RigidBody {
    float mass{1.0f};
    float drag{0.0f};
    float angular_drag{0.05f};
    bool use_gravity{true};
    bool is_kinematic{false};
};

// ============================================================================
// Rendering Components
// ============================================================================

// Mesh reference for rendering
struct MeshComponent {
    Mesh mesh{};
    Material material{};
    bool cast_shadow{true};
};

// Model reference for more complex objects
struct ModelComponent {
    Model model{};
    bool visible{true};
};

// ============================================================================
// Voxel/Block Components
// ============================================================================

// Current tool held by entity
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

// Block breaking progress
struct BlockBreaker {
    bool is_breaking{false};
    float break_progress{0.0f};
    int target_block_x{0};
    int target_block_y{0};
    int target_block_z{0};
    bool has_target{false};
};

// ============================================================================
// Input Components
// ============================================================================

// Input state for an entity
struct InputState {
    Vector2 move_input{0.0f, 0.0f};
    Vector2 look_input{0.0f, 0.0f};
    bool jump_pressed{false};
    bool sprint_pressed{false};
    bool primary_action{false};   // Left mouse
    bool secondary_action{false}; // Right mouse
};

// ============================================================================
// Utility Components
// ============================================================================

// Name tag for debugging
struct NameTag {
    const char* name{nullptr};
};

// Lifetime component for temporary entities
struct Lifetime {
    float remaining{0.0f};
};

// Active/inactive state
struct Active {
    bool value{true};
};

} // namespace ecs
