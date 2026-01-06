#pragma once

// =============================================================================
// Collision2D System - 2D collision detection and response
// =============================================================================
//
// Detects collisions between entities with colliders (CircleCollider, BoxCollider2D).
// Provides collision events that can be queried by user systems.
//
// Usage:
//   Collision2DSystem collision;
//   collision.update(registry, dt);
//   
//   // Check collisions for an entity
//   for (auto& hit : collision.get_collisions(entity)) {
//       // handle collision with hit.other
//   }
//

#include "../system.hpp"
#include "../components/common.hpp"

#include <vector>
#include <cmath>
#include <algorithm>

namespace ecs {

/// Collision event data
struct CollisionHit {
    entt::entity self{entt::null};
    entt::entity other{entt::null};
    float overlap_x{0.0f};
    float overlap_y{0.0f};
    float normal_x{0.0f};
    float normal_y{0.0f};
    bool is_trigger{false};
};

class Collision2DSystem : public System {
public:
    void update(entt::registry& registry, float dt) override {
        (void)dt;
        
        collisions_.clear();
        
        // Circle vs Circle
        check_circle_circle(registry);
        
        // Box vs Box  
        check_box_box(registry);
        
        // Circle vs Box
        check_circle_box(registry);
    }
    
    /// Get all collisions from last update
    const std::vector<CollisionHit>& get_collisions() const {
        return collisions_;
    }
    
    /// Get collisions for a specific entity
    std::vector<CollisionHit> get_collisions_for(entt::entity entity) const {
        std::vector<CollisionHit> result;
        for (const auto& hit : collisions_) {
            if (hit.self == entity || hit.other == entity) {
                result.push_back(hit);
            }
        }
        return result;
    }
    
    /// Check if two entities are colliding
    bool are_colliding(entt::entity a, entt::entity b) const {
        for (const auto& hit : collisions_) {
            if ((hit.self == a && hit.other == b) || (hit.self == b && hit.other == a)) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<CollisionHit> collisions_;
    
    bool layers_collide(const CollisionLayer* a, const CollisionLayer* b) const {
        if (!a || !b) return true;  // no layer = collide with everything
        return (a->layer & b->mask) && (b->layer & a->mask);
    }
    
    void check_circle_circle(entt::registry& registry) {
        auto view = registry.view<Transform2D, CircleCollider>();
        auto entities = std::vector<entt::entity>(view.begin(), view.end());
        
        for (size_t i = 0; i < entities.size(); ++i) {
            for (size_t j = i + 1; j < entities.size(); ++j) {
                auto e1 = entities[i];
                auto e2 = entities[j];
                
                auto* layer1 = registry.try_get<CollisionLayer>(e1);
                auto* layer2 = registry.try_get<CollisionLayer>(e2);
                if (!layers_collide(layer1, layer2)) continue;
                
                const auto& t1 = view.get<Transform2D>(e1);
                const auto& c1 = view.get<CircleCollider>(e1);
                const auto& t2 = view.get<Transform2D>(e2);
                const auto& c2 = view.get<CircleCollider>(e2);
                
                float x1 = t1.x + c1.offset_x;
                float y1 = t1.y + c1.offset_y;
                float x2 = t2.x + c2.offset_x;
                float y2 = t2.y + c2.offset_y;
                
                float dx = x2 - x1;
                float dy = y2 - y1;
                float dist_sq = dx * dx + dy * dy;
                float radius_sum = c1.radius + c2.radius;
                
                if (dist_sq < radius_sum * radius_sum) {
                    float dist = std::sqrt(dist_sq);
                    float overlap = radius_sum - dist;
                    
                    float nx = (dist > 0.0f) ? dx / dist : 1.0f;
                    float ny = (dist > 0.0f) ? dy / dist : 0.0f;
                    
                    CollisionHit hit;
                    hit.self = e1;
                    hit.other = e2;
                    hit.overlap_x = nx * overlap;
                    hit.overlap_y = ny * overlap;
                    hit.normal_x = nx;
                    hit.normal_y = ny;
                    hit.is_trigger = c1.is_trigger || c2.is_trigger;
                    collisions_.push_back(hit);
                }
            }
        }
    }
    
    void check_box_box(entt::registry& registry) {
        auto view = registry.view<Transform2D, BoxCollider2D>();
        auto entities = std::vector<entt::entity>(view.begin(), view.end());
        
        for (size_t i = 0; i < entities.size(); ++i) {
            for (size_t j = i + 1; j < entities.size(); ++j) {
                auto e1 = entities[i];
                auto e2 = entities[j];
                
                auto* layer1 = registry.try_get<CollisionLayer>(e1);
                auto* layer2 = registry.try_get<CollisionLayer>(e2);
                if (!layers_collide(layer1, layer2)) continue;
                
                const auto& t1 = view.get<Transform2D>(e1);
                const auto& b1 = view.get<BoxCollider2D>(e1);
                const auto& t2 = view.get<Transform2D>(e2);
                const auto& b2 = view.get<BoxCollider2D>(e2);
                
                float x1 = t1.x + b1.offset_x;
                float y1 = t1.y + b1.offset_y;
                float x2 = t2.x + b2.offset_x;
                float y2 = t2.y + b2.offset_y;
                
                float half_w1 = b1.width * 0.5f;
                float half_h1 = b1.height * 0.5f;
                float half_w2 = b2.width * 0.5f;
                float half_h2 = b2.height * 0.5f;
                
                float dx = x2 - x1;
                float dy = y2 - y1;
                float overlap_x = (half_w1 + half_w2) - std::abs(dx);
                float overlap_y = (half_h1 + half_h2) - std::abs(dy);
                
                if (overlap_x > 0 && overlap_y > 0) {
                    CollisionHit hit;
                    hit.self = e1;
                    hit.other = e2;
                    hit.is_trigger = b1.is_trigger || b2.is_trigger;
                    
                    // Use minimum overlap axis
                    if (overlap_x < overlap_y) {
                        hit.normal_x = (dx > 0) ? 1.0f : -1.0f;
                        hit.normal_y = 0.0f;
                        hit.overlap_x = hit.normal_x * overlap_x;
                        hit.overlap_y = 0.0f;
                    } else {
                        hit.normal_x = 0.0f;
                        hit.normal_y = (dy > 0) ? 1.0f : -1.0f;
                        hit.overlap_x = 0.0f;
                        hit.overlap_y = hit.normal_y * overlap_y;
                    }
                    
                    collisions_.push_back(hit);
                }
            }
        }
    }
    
    void check_circle_box(entt::registry& registry) {
        auto circle_view = registry.view<Transform2D, CircleCollider>();
        auto box_view = registry.view<Transform2D, BoxCollider2D>();
        
        for (auto circle_entity : circle_view) {
            const auto& ct = circle_view.get<Transform2D>(circle_entity);
            const auto& cc = circle_view.get<CircleCollider>(circle_entity);
            auto* layer_c = registry.try_get<CollisionLayer>(circle_entity);
            
            float cx = ct.x + cc.offset_x;
            float cy = ct.y + cc.offset_y;
            
            for (auto box_entity : box_view) {
                if (circle_entity == box_entity) continue;
                
                // Skip if has both colliders (handled in other checks)
                if (registry.all_of<CircleCollider>(box_entity)) continue;
                
                auto* layer_b = registry.try_get<CollisionLayer>(box_entity);
                if (!layers_collide(layer_c, layer_b)) continue;
                
                const auto& bt = box_view.get<Transform2D>(box_entity);
                const auto& bc = box_view.get<BoxCollider2D>(box_entity);
                
                float bx = bt.x + bc.offset_x;
                float by = bt.y + bc.offset_y;
                float half_w = bc.width * 0.5f;
                float half_h = bc.height * 0.5f;
                
                // Find closest point on box to circle center
                float closest_x = std::clamp(cx, bx - half_w, bx + half_w);
                float closest_y = std::clamp(cy, by - half_h, by + half_h);
                
                float dx = cx - closest_x;
                float dy = cy - closest_y;
                float dist_sq = dx * dx + dy * dy;
                
                if (dist_sq < cc.radius * cc.radius) {
                    float dist = std::sqrt(dist_sq);
                    float overlap = cc.radius - dist;
                    
                    float nx = (dist > 0.0f) ? dx / dist : 1.0f;
                    float ny = (dist > 0.0f) ? dy / dist : 0.0f;
                    
                    CollisionHit hit;
                    hit.self = circle_entity;
                    hit.other = box_entity;
                    hit.overlap_x = nx * overlap;
                    hit.overlap_y = ny * overlap;
                    hit.normal_x = nx;
                    hit.normal_y = ny;
                    hit.is_trigger = cc.is_trigger || bc.is_trigger;
                    collisions_.push_back(hit);
                }
            }
        }
    }
};

} // namespace ecs

