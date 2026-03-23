#pragma once

#include "engine/core/math_types.hpp"
#include "engine/core/export.hpp"
#include <entt/entt.hpp>

#include "engine/core/player_constants.hpp"

namespace rf {
    class IMesh;
    class IShader;
}

namespace ecs {

struct Transform {
    rf::Vec3 position{0.0f, 0.0f, 0.0f};
    rf::Vec3 rotation{0.0f, 0.0f, 0.0f};  // Euler angles (pitch, yaw, roll)
    rf::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct Velocity {
    rf::Vec3 linear{0.0f, 0.0f, 0.0f};
    rf::Vec3 angular{0.0f, 0.0f, 0.0f};
};

struct PreviousPosition {
    rf::Vec3 value{0.0f, 0.0f, 0.0f};
    bool initialized{false};
};

struct BoxCollider {
    rf::Vec3 size{1.0f, 1.0f, 1.0f};
    rf::Vec3 offset{0.0f, 0.0f, 0.0f};
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

// ECS mesh/model components with material pipeline.
// mesh/shader pointers are non-owning — the resource system owns the lifetime.

struct MeshComponent {
    rf::IMesh*    mesh{nullptr};      // If null, RenderSystem uses default cube
    rf::IShader*  shader{nullptr};    // If null, RenderSystem uses solid-color shader
    rf::Color     color{rf::Color::White()};
    bool          cast_shadow{true};
    bool          visible{true};
};

struct ModelComponent {
    rf::IMesh*    mesh{nullptr};
    rf::IShader*  shader{nullptr};
    rf::Color     color{rf::Color::White()};
    bool          visible{true};
};

struct RAYFLOW_CLIENT_API ToolHolder {
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
    rf::Vec2 move_input{0.0f, 0.0f};
    rf::Vec2 look_input{0.0f, 0.0f};
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
