#pragma once

// =============================================================================
// Particle System - 2D particle effects (blood, sparks, smoke, etc.)
// =============================================================================
//
// Updates and renders particle emitters attached to entities.
//
// Usage:
//   ParticleSystem particles;
//   particles.update(registry, dt);
//   
//   BeginMode2D(camera);
//   particles.render(registry);
//   EndMode2D();
//
//   // Spawn burst of particles
//   particles.emit_burst(registry, entity, 20);
//

#include "../system.hpp"
#include "../components/common.hpp"
#include "../components/rendering.hpp"

#include <raylib.h>
#include <cmath>
#include <random>

namespace ecs {

class ParticleSystem : public System {
public:
    void update(entt::registry& registry, float dt) override {
        auto view = registry.view<Transform2D, ParticleEmitter>();
        
        for (auto [entity, transform, emitter] : view.each()) {
            // Update existing particles
            update_particles(emitter, dt);
            
            // Emit new particles
            if (emitter.emitting && !emitter.one_shot) {
                emit_continuous(transform, emitter, dt);
            }
        }
        
        // Update trail effects
        update_trails(registry, dt);
    }
    
    void render(entt::registry& registry) {
        // Render particles
        auto view = registry.view<ParticleEmitter>();
        for (auto [entity, emitter] : view.each()) {
            render_emitter(emitter);
        }
        
        // Render trails
        auto trail_view = registry.view<Transform2D, TrailEffect>();
        for (auto [entity, transform, trail] : trail_view.each()) {
            render_trail(trail);
        }
    }
    
    /// Emit a burst of particles
    void emit_burst(entt::registry& registry, entt::entity entity, int count = -1) {
        auto* transform = registry.try_get<Transform2D>(entity);
        auto* emitter = registry.try_get<ParticleEmitter>(entity);
        if (!transform || !emitter) return;
        
        int to_emit = (count > 0) ? count : emitter->burst_count;
        
        for (int i = 0; i < to_emit; ++i) {
            emit_particle(*transform, *emitter);
        }
    }
    
    /// Create blood splatter effect at position
    void spawn_blood(entt::registry& registry, entt::entity entity, 
                    float x, float y, float direction, int count = 15) {
        auto* emitter = registry.try_get<ParticleEmitter>(entity);
        if (!emitter) return;
        
        // Configure for blood
        emitter->color_start = {180, 20, 20, 255};
        emitter->color_end = {80, 10, 10, 0};
        emitter->lifetime_min = 0.3f;
        emitter->lifetime_max = 0.8f;
        emitter->speed_min = 100.0f;
        emitter->speed_max = 300.0f;
        emitter->direction = direction;
        emitter->spread = 0.8f;
        emitter->size_min = 2.0f;
        emitter->size_max = 6.0f;
        emitter->gravity = 300.0f;
        emitter->offset_x = x;
        emitter->offset_y = y;
        
        Transform2D temp{0, 0, 0};
        for (int i = 0; i < count; ++i) {
            emit_particle(temp, *emitter);
        }
    }
    
    /// Create spark effect
    void spawn_sparks(entt::registry& registry, entt::entity entity,
                     float x, float y, int count = 10) {
        auto* emitter = registry.try_get<ParticleEmitter>(entity);
        if (!emitter) return;
        
        emitter->color_start = {255, 200, 50, 255};
        emitter->color_end = {255, 100, 0, 0};
        emitter->lifetime_min = 0.1f;
        emitter->lifetime_max = 0.3f;
        emitter->speed_min = 200.0f;
        emitter->speed_max = 400.0f;
        emitter->direction = 0;
        emitter->spread = 3.14159f;
        emitter->size_min = 1.0f;
        emitter->size_max = 3.0f;
        emitter->gravity = 200.0f;
        emitter->offset_x = x;
        emitter->offset_y = y;
        
        Transform2D temp{0, 0, 0};
        for (int i = 0; i < count; ++i) {
            emit_particle(temp, *emitter);
        }
    }

private:
    std::mt19937 rng_{std::random_device{}()};
    
    void update_particles(ParticleEmitter& emitter, float dt) {
        emitter.active_count = 0;
        
        for (int i = 0; i < ParticleEmitter::kMaxParticles; ++i) {
            auto& p = emitter.particles[i];
            if (!p.active) continue;
            
            p.life -= dt;
            if (p.life <= 0) {
                p.active = false;
                continue;
            }
            
            // Apply gravity
            p.vy += emitter.gravity * dt;
            
            // Move
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.rotation += p.angular_velocity * dt;
            
            // Interpolate color
            float t = 1.0f - (p.life / p.max_life);
            p.color = lerp_color(emitter.color_start, emitter.color_end, t);
            
            // Interpolate size
            p.size = lerp(emitter.size_max, emitter.size_end, t);
            
            emitter.active_count++;
        }
    }
    
    void emit_continuous(const Transform2D& transform, ParticleEmitter& emitter, float dt) {
        emitter.emit_timer += dt;
        float interval = 1.0f / emitter.emit_rate;
        
        while (emitter.emit_timer >= interval) {
            emitter.emit_timer -= interval;
            emit_particle(transform, emitter);
        }
    }
    
    void emit_particle(const Transform2D& transform, ParticleEmitter& emitter) {
        // Find inactive particle slot
        int slot = -1;
        for (int i = 0; i < ParticleEmitter::kMaxParticles; ++i) {
            if (!emitter.particles[i].active) {
                slot = i;
                break;
            }
        }
        if (slot < 0) return;  // no free slots
        
        auto& p = emitter.particles[slot];
        p.active = true;
        
        // Position
        p.x = transform.x + emitter.offset_x;
        p.y = transform.y + emitter.offset_y;
        
        // Velocity
        float angle = emitter.direction + random_float(-emitter.spread, emitter.spread);
        float speed = random_float(emitter.speed_min, emitter.speed_max);
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        
        // Lifetime
        p.max_life = random_float(emitter.lifetime_min, emitter.lifetime_max);
        p.life = p.max_life;
        
        // Size
        p.size = random_float(emitter.size_min, emitter.size_max);
        
        // Rotation
        p.rotation = random_float(0, 6.28318f);
        p.angular_velocity = random_float(-5.0f, 5.0f);
        
        // Color
        p.color = emitter.color_start;
        p.end_color = emitter.color_end;
    }
    
    void render_emitter(const ParticleEmitter& emitter) {
        for (int i = 0; i < ParticleEmitter::kMaxParticles; ++i) {
            const auto& p = emitter.particles[i];
            if (!p.active) continue;
            
            DrawCircleV({p.x, p.y}, p.size, p.color);
        }
    }
    
    void update_trails(entt::registry& registry, float dt) {
        auto view = registry.view<Transform2D, TrailEffect>();
        for (auto [entity, transform, trail] : view.each()) {
            if (!trail.enabled) continue;
            
            trail.timer += dt;
            if (trail.timer >= trail.point_interval) {
                trail.timer = 0.0f;
                
                // Add new point
                trail.points[trail.head] = {transform.x, transform.y};
                trail.head = (trail.head + 1) % TrailEffect::kMaxPoints;
                if (trail.point_count < TrailEffect::kMaxPoints) {
                    trail.point_count++;
                }
            }
        }
    }
    
    void render_trail(const TrailEffect& trail) {
        if (trail.point_count < 2) return;
        
        for (int i = 0; i < trail.point_count - 1; ++i) {
            int idx1 = (trail.head - trail.point_count + i + TrailEffect::kMaxPoints) % TrailEffect::kMaxPoints;
            int idx2 = (idx1 + 1) % TrailEffect::kMaxPoints;
            
            float t = static_cast<float>(i) / static_cast<float>(trail.point_count - 1);
            float width = lerp(trail.width_end, trail.width_start, t);
            Color color = lerp_color(trail.color_end, trail.color_start, t);
            
            DrawLineEx(trail.points[idx1], trail.points[idx2], width, color);
        }
    }
    
    float random_float(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng_);
    }
    
    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
    
    static Color lerp_color(Color a, Color b, float t) {
        return {
            static_cast<unsigned char>(a.r + (b.r - a.r) * t),
            static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t),
            static_cast<unsigned char>(a.a + (b.a - a.a) * t)
        };
    }
};

} // namespace ecs

