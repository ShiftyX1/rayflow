#pragma once

// =============================================================================
// Physics2D System - 2D movement and velocity integration
// =============================================================================
//
// Updates Transform2D based on Velocity2D and Movement2D components.
// Does NOT handle collisions - use CollisionSystem for that.
//
// Usage:
//   Physics2DSystem physics;
//   physics.update(registry, dt);
//

#include "../system.hpp"
#include "../components/common.hpp"

#include <cmath>

namespace ecs {

class Physics2DSystem : public System {
public:
    void update(entt::registry& registry, float dt) override {
        // Apply acceleration to velocity
        apply_acceleration(registry, dt);
        
        // Apply friction/drag
        apply_friction(registry, dt);
        
        // Clamp to max speed
        clamp_speed(registry);
        
        // Integrate velocity into position
        integrate_velocity(registry, dt);
    }

private:
    void apply_acceleration(entt::registry& registry, float dt) {
        auto view = registry.view<Velocity2D, const Acceleration2D>();
        for (auto [entity, vel, accel] : view.each()) {
            vel.vx += accel.ax * dt;
            vel.vy += accel.ay * dt;
        }
    }
    
    void apply_friction(entt::registry& registry, float dt) {
        auto view = registry.view<Velocity2D, const Movement2D>();
        for (auto [entity, vel, move] : view.each()) {
            if (move.friction > 0.0f) {
                float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);
                if (speed > 0.0f) {
                    float friction_force = move.friction * dt;
                    float new_speed = std::max(0.0f, speed - friction_force);
                    float scale = new_speed / speed;
                    vel.vx *= scale;
                    vel.vy *= scale;
                }
            }
        }
    }
    
    void clamp_speed(entt::registry& registry) {
        auto view = registry.view<Velocity2D, const Movement2D>();
        for (auto [entity, vel, move] : view.each()) {
            float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);
            if (speed > move.max_speed) {
                float scale = move.max_speed / speed;
                vel.vx *= scale;
                vel.vy *= scale;
            }
        }
    }
    
    void integrate_velocity(entt::registry& registry, float dt) {
        auto view = registry.view<Transform2D, const Velocity2D>();
        for (auto [entity, transform, vel] : view.each()) {
            transform.x += vel.vx * dt;
            transform.y += vel.vy * dt;
            transform.rotation += vel.angular * dt;
        }
    }
};

} // namespace ecs

