#pragma once

// =============================================================================
// Sprite System - 2D sprite rendering with animations
// =============================================================================
//
// Renders Sprite and AnimatedSprite components.
// Must be called inside BeginMode2D()/EndMode2D() or with a camera.
//
// Usage:
//   SpriteSystem sprites;
//   
//   BeginMode2D(camera);
//   sprites.render(registry);
//   EndMode2D();
//   
//   // Update animations each frame
//   sprites.update(registry, dt);
//

#include "../system.hpp"
#include "../components/common.hpp"
#include "../components/rendering.hpp"

#include <raylib.h>
#include <vector>
#include <algorithm>

namespace ecs {

class SpriteSystem : public System {
public:
    /// Update animations (call every frame)
    void update(entt::registry& registry, float dt) override {
        update_animations(registry, dt);
        update_flash_effects(registry, dt);
    }
    
    /// Render all sprites (call inside BeginMode2D)
    void render(entt::registry& registry) {
        // Collect all renderable entities with z-order
        render_queue_.clear();
        
        // Static sprites
        auto sprite_view = registry.view<Transform2D, Sprite>();
        for (auto entity : sprite_view) {
            const auto& sprite = sprite_view.get<Sprite>(entity);
            render_queue_.push_back({entity, sprite.z_order, false});
        }
        
        // Animated sprites
        auto anim_view = registry.view<Transform2D, AnimatedSprite>();
        for (auto entity : anim_view) {
            // Skip if also has static sprite (avoid double render)
            if (registry.all_of<Sprite>(entity)) continue;
            
            const auto& anim = anim_view.get<AnimatedSprite>(entity);
            render_queue_.push_back({entity, anim.z_order, true});
        }
        
        // Sort by z-order
        std::sort(render_queue_.begin(), render_queue_.end(),
            [](const RenderItem& a, const RenderItem& b) {
                return a.z_order < b.z_order;
            });
        
        // Render in order
        for (const auto& item : render_queue_) {
            if (item.is_animated) {
                render_animated_sprite(registry, item.entity);
            } else {
                render_sprite(registry, item.entity);
            }
        }
    }

private:
    struct RenderItem {
        entt::entity entity;
        int z_order;
        bool is_animated;
    };
    
    std::vector<RenderItem> render_queue_;
    
    void update_animations(entt::registry& registry, float dt) {
        auto view = registry.view<AnimatedSprite>();
        for (auto [entity, anim] : view.each()) {
            if (!anim.playing) continue;
            
            anim.timer += dt;
            if (anim.timer >= anim.frame_time) {
                anim.timer -= anim.frame_time;
                anim.frame++;
                
                if (anim.frame >= anim.frame_count) {
                    if (anim.loop) {
                        anim.frame = 0;
                    } else {
                        anim.frame = anim.frame_count - 1;
                        anim.playing = false;
                    }
                }
            }
        }
        
        // Update animation sets
        auto set_view = registry.view<AnimatedSprite, AnimationSet>();
        for (auto [entity, anim, set] : set_view.each()) {
            if (set.current_animation < set.animation_count) {
                const auto& current = set.animations[set.current_animation];
                
                // Map global frame to animation frame
                int local_frame = anim.frame - current.start_frame;
                if (local_frame < 0 || local_frame >= current.frame_count) {
                    anim.frame = current.start_frame;
                }
            }
        }
    }
    
    void update_flash_effects(entt::registry& registry, float dt) {
        auto view = registry.view<FlashEffect>();
        for (auto [entity, flash] : view.each()) {
            if (flash.active) {
                flash.timer -= dt;
                if (flash.timer <= 0.0f) {
                    flash.active = false;
                }
            }
        }
    }
    
    void render_sprite(entt::registry& registry, entt::entity entity) {
        const auto& transform = registry.get<Transform2D>(entity);
        const auto& sprite = registry.get<Sprite>(entity);
        
        if (sprite.texture.id == 0) return;  // no texture
        
        Rectangle source = sprite.source;
        if (source.width == 0 || source.height == 0) {
            source = {0, 0, 
                static_cast<float>(sprite.texture.width),
                static_cast<float>(sprite.texture.height)};
        }
        
        if (sprite.flip_x) source.width = -source.width;
        if (sprite.flip_y) source.height = -source.height;
        
        Rectangle dest = {
            transform.x,
            transform.y,
            std::abs(source.width) * sprite.scale,
            std::abs(source.height) * sprite.scale
        };
        
        Color tint = sprite.tint;
        auto* flash = registry.try_get<FlashEffect>(entity);
        if (flash && flash->active) {
            tint = flash->color;
        }
        
        DrawTexturePro(sprite.texture, source, dest, sprite.origin,
                      transform.rotation * RAD2DEG, tint);
    }
    
    void render_animated_sprite(entt::registry& registry, entt::entity entity) {
        const auto& transform = registry.get<Transform2D>(entity);
        const auto& anim = registry.get<AnimatedSprite>(entity);
        
        if (anim.spritesheet.id == 0) return;  // no texture
        if (anim.frame_width == 0 || anim.frame_height == 0) return;
        
        int col = anim.frame % anim.frames_per_row;
        int row = anim.frame / anim.frames_per_row;
        
        Rectangle source = {
            static_cast<float>(col * anim.frame_width),
            static_cast<float>(row * anim.frame_height),
            static_cast<float>(anim.frame_width),
            static_cast<float>(anim.frame_height)
        };
        
        if (anim.flip_x) source.width = -source.width;
        if (anim.flip_y) source.height = -source.height;
        
        Rectangle dest = {
            transform.x,
            transform.y,
            std::abs(source.width) * anim.scale,
            std::abs(source.height) * anim.scale
        };
        
        Color tint = anim.tint;
        auto* flash = registry.try_get<FlashEffect>(entity);
        if (flash && flash->active) {
            tint = flash->color;
        }
        
        DrawTexturePro(anim.spritesheet, source, dest, anim.origin,
                      transform.rotation * RAD2DEG, tint);
    }
};

// =============================================================================
// Helper functions for sprite manipulation
// =============================================================================

/// Play a specific animation from AnimationSet
inline void play_animation(AnimatedSprite& sprite, AnimationSet& set, int animation_index) {
    if (animation_index < 0 || animation_index >= set.animation_count) return;
    
    set.current_animation = animation_index;
    const auto& anim = set.animations[animation_index];
    
    sprite.frame = anim.start_frame;
    sprite.frame_count = anim.start_frame + anim.frame_count;
    sprite.frame_time = anim.frame_time;
    sprite.loop = anim.loop;
    sprite.playing = true;
    sprite.timer = 0.0f;
}

/// Trigger a flash effect
inline void trigger_flash(FlashEffect& flash, Color color, float duration) {
    flash.color = color;
    flash.duration = duration;
    flash.timer = duration;
    flash.active = true;
}

} // namespace ecs

