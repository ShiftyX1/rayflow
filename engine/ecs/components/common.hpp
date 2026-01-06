#pragma once

// =============================================================================
// Common ECS Components - Universal game components (headless, no raylib)
// =============================================================================
// 
// Contract: Composition over Inheritance
// - Components are POD structs (data only, no virtual functions)
// - Users extend by adding their own components to entities
// - Engine systems work with these base components
// - User systems can combine engine + custom components
//
// Example:
//   auto entity = registry.create();
//   registry.emplace<ecs::Health>(entity, 100, 100);
//   registry.emplace<ecs::Transform2D>(entity, 0.f, 0.f, 0.f);
//   registry.emplace<MyCustomShield>(entity, 50);  // User's component
//

#include <cstdint>

namespace ecs {

// =============================================================================
// Health & Combat
// =============================================================================

/// Basic health component
struct Health {
    int current{100};
    int max{100};
};

/// Damage multiplier (e.g., 0.5 = takes 50% damage)
struct DamageMultiplier {
    float value{1.0f};
};

/// Invulnerability frames
struct Invulnerable {
    float remaining{0.0f};  // seconds remaining
};

// =============================================================================
// Weapons
// =============================================================================

/// Weapon component for melee and ranged weapons
struct Weapon {
    enum class Type : std::uint8_t {
        Melee,
        Pistol,
        Shotgun,
        SMG,
        Rifle,
        Bat,
        Knife
    };
    
    Type type{Type::Melee};
    float damage{10.0f};
    float range{32.0f};         // pixels for melee, or max distance for ranged
    float cooldown{0.5f};       // seconds between attacks
    float spread{0.0f};         // radians, for ranged weapons
    int ammo{-1};               // -1 = infinite
    int mag_size{-1};           // -1 = no reloading
    float reload_time{1.0f};    // seconds
};

/// Current weapon state
struct WeaponState {
    float cooldown_remaining{0.0f};
    float reload_remaining{0.0f};
    int current_ammo{0};
    bool is_reloading{false};
};

// =============================================================================
// AI State Machine
// =============================================================================

/// AI controller with FSM states
struct AIController {
    enum class State : std::uint8_t {
        Idle,
        Patrol,
        Chase,
        Attack,
        Flee,
        Dead
    };
    
    State state{State::Idle};
    float sight_range{200.0f};      // pixels
    float attack_range{32.0f};      // pixels
    float reaction_time{0.2f};      // seconds before reacting
    float state_timer{0.0f};        // time in current state
};

/// AI target (which entity to chase/attack)
struct AITarget {
    std::uint32_t entity_id{0};     // entt::entity as uint32
    bool has_target{false};
};

/// Patrol waypoints for AI
struct PatrolPath {
    static constexpr int kMaxWaypoints = 8;
    float waypoints_x[kMaxWaypoints]{};
    float waypoints_y[kMaxWaypoints]{};
    int waypoint_count{0};
    int current_waypoint{0};
};

// =============================================================================
// 2D Transform & Physics (headless)
// =============================================================================

/// 2D position and rotation
struct Transform2D {
    float x{0.0f};
    float y{0.0f};
    float rotation{0.0f};  // radians
};

/// 2D velocity
struct Velocity2D {
    float vx{0.0f};
    float vy{0.0f};
    float angular{0.0f};   // radians per second
};

/// 2D acceleration (for forces)
struct Acceleration2D {
    float ax{0.0f};
    float ay{0.0f};
};

/// Movement parameters
struct Movement2D {
    float max_speed{200.0f};       // pixels per second
    float acceleration{1000.0f};   // pixels per second^2
    float friction{500.0f};        // deceleration when no input
};

// =============================================================================
// Colliders (2D)
// =============================================================================

/// Circle collider
struct CircleCollider {
    float radius{8.0f};
    float offset_x{0.0f};
    float offset_y{0.0f};
    bool is_trigger{false};  // triggers don't block movement
};

/// Axis-aligned box collider
struct BoxCollider2D {
    float width{16.0f};
    float height{16.0f};
    float offset_x{0.0f};
    float offset_y{0.0f};
    bool is_trigger{false};
};

/// Collision layer for filtering
struct CollisionLayer {
    std::uint32_t layer{1};       // which layer this entity is on
    std::uint32_t mask{0xFFFFFFFF}; // which layers to collide with
};

// =============================================================================
// Tags & Markers
// =============================================================================

/// Tag for player-controlled entities
struct PlayerTag {};

/// Tag for enemy entities
struct EnemyTag {};

/// Tag for projectiles
struct ProjectileTag {};

/// Tag for dead entities (pending removal)
struct DeadTag {};

/// Entity lifetime (auto-destroy after time)
struct Lifetime {
    float remaining{1.0f};  // seconds
};

/// Active/enabled flag
struct Active {
    bool value{true};
};

// =============================================================================
// Team/Faction
// =============================================================================

/// Team identifier
struct Team {
    std::uint8_t id{0};  // 0 = neutral, 1+ = teams
};

} // namespace ecs
