#pragma once

// =============================================================================
// AI System - Simple FSM-based AI for enemies
// =============================================================================
//
// Provides basic AI behaviors: idle, patrol, chase, attack, flee.
// Users can extend by creating their own systems that modify AIController state.
//
// Usage:
//   AISystem ai;
//   ai.set_player_entity(player);  // who to chase
//   ai.update(registry, dt);
//

#include "../system.hpp"
#include "../components/common.hpp"

#include <cmath>
#include <random>

namespace ecs {

class AISystem : public System {
public:
    /// Set the entity that AI should consider as "player" (target)
    void set_player_entity(entt::entity player) {
        player_entity_ = player;
        has_player_ = true;
    }
    
    void clear_player_entity() {
        has_player_ = false;
    }
    
    void update(entt::registry& registry, float dt) override {
        // Update state timers
        update_timers(registry, dt);
        
        // Find targets
        if (has_player_) {
            update_target_detection(registry);
        }
        
        // Process state transitions
        process_state_transitions(registry);
        
        // Execute current state behavior
        execute_state_behaviors(registry, dt);
    }

private:
    entt::entity player_entity_{entt::null};
    bool has_player_{false};
    std::mt19937 rng_{std::random_device{}()};
    
    void update_timers(entt::registry& registry, float dt) {
        auto view = registry.view<AIController>();
        for (auto [entity, ai] : view.each()) {
            ai.state_timer += dt;
        }
    }
    
    void update_target_detection(entt::registry& registry) {
        if (!has_player_ || !registry.valid(player_entity_)) return;
        
        auto* player_transform = registry.try_get<Transform2D>(player_entity_);
        if (!player_transform) return;
        
        float player_x = player_transform->x;
        float player_y = player_transform->y;
        
        auto view = registry.view<AIController, AITarget, Transform2D>();
        for (auto [entity, ai, target, transform] : view.each()) {
            if (ai.state == AIController::State::Dead) continue;
            
            float dx = player_x - transform.x;
            float dy = player_y - transform.y;
            float dist_sq = dx * dx + dy * dy;
            float sight_sq = ai.sight_range * ai.sight_range;
            
            if (dist_sq <= sight_sq) {
                target.entity_id = static_cast<std::uint32_t>(player_entity_);
                target.has_target = true;
            } else {
                // Lost sight
                if (ai.state == AIController::State::Chase || 
                    ai.state == AIController::State::Attack) {
                    target.has_target = false;
                }
            }
        }
    }
    
    void process_state_transitions(entt::registry& registry) {
        auto view = registry.view<AIController, Transform2D>();
        
        for (auto [entity, ai, transform] : view.each()) {
            if (ai.state == AIController::State::Dead) continue;
            
            auto* target = registry.try_get<AITarget>(entity);
            auto* health = registry.try_get<Health>(entity);
            
            // Check for death
            if (health && health->current <= 0) {
                change_state(ai, AIController::State::Dead);
                continue;
            }
            
            // Check for flee (low health)
            if (health && health->current < health->max * 0.2f) {
                if (ai.state != AIController::State::Flee) {
                    change_state(ai, AIController::State::Flee);
                    continue;
                }
            }
            
            bool has_target = target && target->has_target;
            float dist_to_target = 0.0f;
            
            if (has_target && has_player_ && registry.valid(player_entity_)) {
                auto* player_t = registry.try_get<Transform2D>(player_entity_);
                if (player_t) {
                    float dx = player_t->x - transform.x;
                    float dy = player_t->y - transform.y;
                    dist_to_target = std::sqrt(dx * dx + dy * dy);
                }
            }
            
            switch (ai.state) {
                case AIController::State::Idle:
                    if (has_target) {
                        if (ai.state_timer >= ai.reaction_time) {
                            change_state(ai, AIController::State::Chase);
                        }
                    } else if (ai.state_timer > 3.0f) {
                        // Random chance to patrol
                        if (random_chance(0.3f)) {
                            change_state(ai, AIController::State::Patrol);
                        } else {
                            ai.state_timer = 0.0f;  // reset idle timer
                        }
                    }
                    break;
                    
                case AIController::State::Patrol:
                    if (has_target) {
                        change_state(ai, AIController::State::Chase);
                    } else if (ai.state_timer > 5.0f) {
                        change_state(ai, AIController::State::Idle);
                    }
                    break;
                    
                case AIController::State::Chase:
                    if (!has_target) {
                        change_state(ai, AIController::State::Idle);
                    } else if (dist_to_target <= ai.attack_range) {
                        change_state(ai, AIController::State::Attack);
                    }
                    break;
                    
                case AIController::State::Attack:
                    if (!has_target) {
                        change_state(ai, AIController::State::Idle);
                    } else if (dist_to_target > ai.attack_range * 1.5f) {
                        change_state(ai, AIController::State::Chase);
                    }
                    break;
                    
                case AIController::State::Flee:
                    if (health && health->current >= health->max * 0.5f) {
                        change_state(ai, AIController::State::Idle);
                    } else if (ai.state_timer > 5.0f) {
                        change_state(ai, AIController::State::Idle);
                    }
                    break;
                    
                case AIController::State::Dead:
                    // Stay dead
                    break;
            }
        }
    }
    
    void execute_state_behaviors(entt::registry& registry, float dt) {
        (void)dt;
        
        auto view = registry.view<AIController, Transform2D, Velocity2D>();
        
        for (auto [entity, ai, transform, vel] : view.each()) {
            auto* movement = registry.try_get<Movement2D>(entity);
            float speed = movement ? movement->max_speed * 0.6f : 100.0f;
            
            switch (ai.state) {
                case AIController::State::Idle:
                    // Stop moving
                    vel.vx *= 0.9f;
                    vel.vy *= 0.9f;
                    break;
                    
                case AIController::State::Patrol:
                    execute_patrol(registry, entity, ai, transform, vel, speed);
                    break;
                    
                case AIController::State::Chase:
                    execute_chase(registry, entity, transform, vel, speed);
                    break;
                    
                case AIController::State::Attack:
                    // Stop and attack (damage dealt by user's combat system)
                    vel.vx *= 0.8f;
                    vel.vy *= 0.8f;
                    break;
                    
                case AIController::State::Flee:
                    execute_flee(registry, entity, transform, vel, speed * 1.2f);
                    break;
                    
                case AIController::State::Dead:
                    vel.vx = 0;
                    vel.vy = 0;
                    break;
            }
        }
    }
    
    void execute_patrol(entt::registry& registry, entt::entity entity,
                       AIController& ai, Transform2D& transform,
                       Velocity2D& vel, float speed) {
        auto* path = registry.try_get<PatrolPath>(entity);
        if (!path || path->waypoint_count == 0) {
            // No path, random movement
            if (ai.state_timer < 0.1f) {
                float angle = random_float(0.0f, 6.28318f);
                vel.vx = std::cos(angle) * speed;
                vel.vy = std::sin(angle) * speed;
            }
            return;
        }
        
        float target_x = path->waypoints_x[path->current_waypoint];
        float target_y = path->waypoints_y[path->current_waypoint];
        
        float dx = target_x - transform.x;
        float dy = target_y - transform.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist < 10.0f) {
            // Reached waypoint
            path->current_waypoint = (path->current_waypoint + 1) % path->waypoint_count;
        } else {
            vel.vx = (dx / dist) * speed;
            vel.vy = (dy / dist) * speed;
        }
    }
    
    void execute_chase(entt::registry& registry, entt::entity entity,
                      Transform2D& transform, Velocity2D& vel, float speed) {
        if (!has_player_ || !registry.valid(player_entity_)) return;
        
        auto* player_t = registry.try_get<Transform2D>(player_entity_);
        if (!player_t) return;
        
        (void)entity;
        
        float dx = player_t->x - transform.x;
        float dy = player_t->y - transform.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist > 1.0f) {
            vel.vx = (dx / dist) * speed;
            vel.vy = (dy / dist) * speed;
        }
    }
    
    void execute_flee(entt::registry& registry, entt::entity entity,
                     Transform2D& transform, Velocity2D& vel, float speed) {
        if (!has_player_ || !registry.valid(player_entity_)) return;
        
        auto* player_t = registry.try_get<Transform2D>(player_entity_);
        if (!player_t) return;
        
        (void)entity;
        
        float dx = transform.x - player_t->x;  // opposite direction
        float dy = transform.y - player_t->y;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist > 1.0f) {
            vel.vx = (dx / dist) * speed;
            vel.vy = (dy / dist) * speed;
        }
    }
    
    void change_state(AIController& ai, AIController::State new_state) {
        ai.state = new_state;
        ai.state_timer = 0.0f;
    }
    
    bool random_chance(float probability) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng_) < probability;
    }
    
    float random_float(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng_);
    }
};

} // namespace ecs

