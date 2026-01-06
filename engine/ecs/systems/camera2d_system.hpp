#pragma once

// =============================================================================
// Camera2D System - 2D camera with follow, smoothing, and screenshake
// =============================================================================
//
// Manages Camera2D for 2D games with smooth following and screen effects.
//
// Usage:
//   Camera2DSystem cameraSystem;
//   cameraSystem.set_screen_size(1280, 720);
//   
//   // Each frame:
//   cameraSystem.update(registry, dt);
//   
//   BeginMode2D(cameraSystem.get_camera());
//   // ... render game world ...
//   EndMode2D();
//
//   // Trigger screenshake
//   cameraSystem.shake(10.0f);
//

#include "../system.hpp"
#include "../components/common.hpp"
#include "../components/rendering.hpp"

#include <raylib.h>
#include <cmath>
#include <random>

namespace ecs {

class Camera2DSystem : public System {
public:
    Camera2DSystem() {
        camera_.target = {0, 0};
        camera_.offset = {0, 0};
        camera_.rotation = 0.0f;
        camera_.zoom = 1.0f;
    }
    
    void set_screen_size(int width, int height) {
        screen_width_ = width;
        screen_height_ = height;
        camera_.offset = {
            static_cast<float>(width) / 2.0f,
            static_cast<float>(height) / 2.0f
        };
    }
    
    void update(entt::registry& registry, float dt) override {
        update_target(registry, dt);
        update_shake(dt);
        apply_bounds(registry);
    }
    
    /// Get the raylib camera for rendering
    Camera2D get_camera() const {
        Camera2D result = camera_;
        
        // Apply shake offset
        result.target.x += shake_offset_x_;
        result.target.y += shake_offset_y_;
        
        return result;
    }
    
    /// Set camera target entity
    void set_target_entity(entt::entity entity) {
        target_entity_ = entity;
        has_target_ = true;
    }
    
    void clear_target_entity() {
        has_target_ = false;
    }
    
    /// Set camera position directly (no smoothing)
    void set_position(float x, float y) {
        camera_.target = {x, y};
    }
    
    /// Set camera zoom
    void set_zoom(float zoom) {
        camera_.zoom = zoom;
    }
    
    float get_zoom() const { return camera_.zoom; }
    
    /// Add screen shake
    void shake(float intensity) {
        shake_intensity_ = std::max(shake_intensity_, intensity);
    }
    
    /// Convert screen position to world position
    Vector2 screen_to_world(Vector2 screen_pos) const {
        return GetScreenToWorld2D(screen_pos, camera_);
    }
    
    /// Convert world position to screen position
    Vector2 world_to_screen(Vector2 world_pos) const {
        return GetWorldToScreen2D(world_pos, camera_);
    }
    
    /// Get camera target position
    Vector2 get_target() const { return camera_.target; }

private:
    Camera2D camera_{};
    int screen_width_{1280};
    int screen_height_{720};
    
    // Target following
    entt::entity target_entity_{entt::null};
    bool has_target_{false};
    float smoothing_{5.0f};
    
    // Screen shake
    float shake_intensity_{0.0f};
    float shake_decay_{8.0f};
    float shake_frequency_{30.0f};
    float shake_timer_{0.0f};
    float shake_offset_x_{0.0f};
    float shake_offset_y_{0.0f};
    
    std::mt19937 rng_{std::random_device{}()};
    
    void update_target(entt::registry& registry, float dt) {
        Vector2 target_pos = camera_.target;
        float current_smoothing = smoothing_;
        float look_ahead_x = 0.0f;
        float look_ahead_y = 0.0f;
        
        // Get controller settings if available
        auto controller_view = registry.view<Camera2DController>();
        for (auto [entity, ctrl] : controller_view.each()) {
            current_smoothing = ctrl.smoothing;
            camera_.zoom = ctrl.zoom;
            camera_.rotation = ctrl.rotation;
            shake_decay_ = ctrl.shake_decay;
            shake_frequency_ = ctrl.shake_frequency;
            
            if (ctrl.shake_intensity > 0) {
                shake(ctrl.shake_intensity);
                ctrl.shake_intensity = 0;  // consumed
            }
            break;  // only use first controller
        }
        
        // Get target from CameraTarget component
        auto target_view = registry.view<CameraTarget>();
        for (auto [entity, cam_target] : target_view.each()) {
            if (cam_target.has_target) {
                auto target_ent = static_cast<entt::entity>(cam_target.entity_id);
                if (registry.valid(target_ent)) {
                    auto* transform = registry.try_get<Transform2D>(target_ent);
                    if (transform) {
                        target_pos = {transform->x, transform->y};
                        
                        // Look ahead based on velocity
                        if (cam_target.look_ahead_factor > 0) {
                            auto* velocity = registry.try_get<Velocity2D>(target_ent);
                            if (velocity) {
                                look_ahead_x = velocity->vx * cam_target.look_ahead_factor;
                                look_ahead_y = velocity->vy * cam_target.look_ahead_factor;
                            }
                        }
                    }
                }
            }
            break;  // only use first target
        }
        
        // Fallback to set target entity
        if (has_target_ && registry.valid(target_entity_)) {
            auto* transform = registry.try_get<Transform2D>(target_entity_);
            if (transform) {
                target_pos = {transform->x, transform->y};
            }
        }
        
        // Apply look ahead
        target_pos.x += look_ahead_x;
        target_pos.y += look_ahead_y;
        
        // Smooth interpolation
        if (current_smoothing > 0) {
            float t = 1.0f - std::exp(-current_smoothing * dt);
            camera_.target.x += (target_pos.x - camera_.target.x) * t;
            camera_.target.y += (target_pos.y - camera_.target.y) * t;
        } else {
            camera_.target = target_pos;
        }
    }
    
    void update_shake(float dt) {
        if (shake_intensity_ <= 0.01f) {
            shake_intensity_ = 0.0f;
            shake_offset_x_ = 0.0f;
            shake_offset_y_ = 0.0f;
            return;
        }
        
        // Decay shake
        shake_intensity_ *= std::exp(-shake_decay_ * dt);
        
        // Calculate shake offset using perlin-like noise
        shake_timer_ += dt * shake_frequency_;
        
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        shake_offset_x_ = dist(rng_) * shake_intensity_;
        shake_offset_y_ = dist(rng_) * shake_intensity_;
    }
    
    void apply_bounds(entt::registry& registry) {
        auto bounds_view = registry.view<CameraBounds>();
        for (auto [entity, bounds] : bounds_view.each()) {
            if (!bounds.enabled) continue;
            
            float half_width = (screen_width_ / camera_.zoom) / 2.0f;
            float half_height = (screen_height_ / camera_.zoom) / 2.0f;
            
            float min_x = bounds.min_x + half_width;
            float max_x = bounds.max_x - half_width;
            float min_y = bounds.min_y + half_height;
            float max_y = bounds.max_y - half_height;
            
            if (max_x > min_x) {
                camera_.target.x = std::clamp(camera_.target.x, min_x, max_x);
            }
            if (max_y > min_y) {
                camera_.target.y = std::clamp(camera_.target.y, min_y, max_y);
            }
            
            break;  // only use first bounds
        }
    }
};

} // namespace ecs

