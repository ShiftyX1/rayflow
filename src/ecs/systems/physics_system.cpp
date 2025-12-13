#include "physics_system.hpp"
#include "../../voxel/world.hpp"
#include <cmath>

namespace ecs {

void PhysicsSystem::update(entt::registry& registry, float delta_time) {
    apply_gravity(registry, delta_time);
    apply_velocity(registry, delta_time);
    resolve_collisions(registry);
}

void PhysicsSystem::apply_gravity(entt::registry& registry, float delta_time) {
    auto view = registry.view<Velocity, GravityAffected, PlayerController>();
    
    for (auto entity : view) {
        auto& velocity = view.get<Velocity>(entity);
        auto& gravity = view.get<GravityAffected>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        // Don't apply gravity in creative mode or when on ground
        if (!player.in_creative_mode && !player.on_ground) {
            velocity.linear.y -= GRAVITY * gravity.gravity_scale * delta_time;
        }
    }
}

void PhysicsSystem::apply_velocity(entt::registry& registry, float delta_time) {
    auto view = registry.view<Transform, Velocity>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& velocity = view.get<Velocity>(entity);
        
        transform.position.x += velocity.linear.x * delta_time;
        transform.position.y += velocity.linear.y * delta_time;
        transform.position.z += velocity.linear.z * delta_time;
    }
}

void PhysicsSystem::resolve_collisions(entt::registry& registry) {
    if (!world_) return;
    
    auto view = registry.view<Transform, Velocity, BoxCollider, PlayerController>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& velocity = view.get<Velocity>(entity);
        auto& collider = view.get<BoxCollider>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        float half_w = collider.size.x / 2.0f;
        float height = collider.size.y;
        float half_d = collider.size.z / 2.0f;
        
        player.on_ground = false;
        
        // Check Y axis (vertical) - most important for ground detection
        {
            float feet_y = transform.position.y;
            float head_y = transform.position.y + height;
            
            // Ground check
            if (velocity.linear.y <= 0) {
                int check_y = static_cast<int>(std::floor(feet_y - 0.01f));
                for (int bx = static_cast<int>(std::floor(transform.position.x - half_w + 0.01f)); 
                     bx <= static_cast<int>(std::floor(transform.position.x + half_w - 0.01f)); bx++) {
                    for (int bz = static_cast<int>(std::floor(transform.position.z - half_d + 0.01f)); 
                         bz <= static_cast<int>(std::floor(transform.position.z + half_d - 0.01f)); bz++) {
                        if (world_->get_block(bx, check_y, bz) != 0) {
                            transform.position.y = static_cast<float>(check_y + 1);
                            velocity.linear.y = 0;
                            player.on_ground = true;
                        }
                    }
                }
            }
            
            // Ceiling check
            if (velocity.linear.y > 0) {
                int check_y = static_cast<int>(std::floor(head_y + 0.01f));
                for (int bx = static_cast<int>(std::floor(transform.position.x - half_w + 0.01f)); 
                     bx <= static_cast<int>(std::floor(transform.position.x + half_w - 0.01f)); bx++) {
                    for (int bz = static_cast<int>(std::floor(transform.position.z - half_d + 0.01f)); 
                         bz <= static_cast<int>(std::floor(transform.position.z + half_d - 0.01f)); bz++) {
                        if (world_->get_block(bx, check_y, bz) != 0) {
                            transform.position.y = static_cast<float>(check_y) - height;
                            velocity.linear.y = 0;
                        }
                    }
                }
            }
        }
        
        // Check X axis (horizontal)
        {
            float margin = 0.001f;
            for (int by = static_cast<int>(std::floor(transform.position.y)); 
                 by <= static_cast<int>(std::floor(transform.position.y + height - 0.01f)); by++) {
                for (int bz = static_cast<int>(std::floor(transform.position.z - half_d + 0.01f)); 
                     bz <= static_cast<int>(std::floor(transform.position.z + half_d - 0.01f)); bz++) {
                    // +X direction
                    int check_x = static_cast<int>(std::floor(transform.position.x + half_w));
                    if (world_->get_block(check_x, by, bz) != 0) {
                        transform.position.x = static_cast<float>(check_x) - half_w - margin;
                        if (velocity.linear.x > 0) velocity.linear.x = 0;
                    }
                    // -X direction
                    check_x = static_cast<int>(std::floor(transform.position.x - half_w));
                    if (world_->get_block(check_x, by, bz) != 0) {
                        transform.position.x = static_cast<float>(check_x + 1) + half_w + margin;
                        if (velocity.linear.x < 0) velocity.linear.x = 0;
                    }
                }
            }
        }
        
        // Check Z axis (horizontal)
        {
            float margin = 0.001f;
            for (int by = static_cast<int>(std::floor(transform.position.y)); 
                 by <= static_cast<int>(std::floor(transform.position.y + height - 0.01f)); by++) {
                for (int bx = static_cast<int>(std::floor(transform.position.x - half_w + 0.01f)); 
                     bx <= static_cast<int>(std::floor(transform.position.x + half_w - 0.01f)); bx++) {
                    // +Z direction
                    int check_z = static_cast<int>(std::floor(transform.position.z + half_d));
                    if (world_->get_block(bx, by, check_z) != 0) {
                        transform.position.z = static_cast<float>(check_z) - half_d - margin;
                        if (velocity.linear.z > 0) velocity.linear.z = 0;
                    }
                    // -Z direction
                    check_z = static_cast<int>(std::floor(transform.position.z - half_d));
                    if (world_->get_block(bx, by, check_z) != 0) {
                        transform.position.z = static_cast<float>(check_z + 1) + half_d + margin;
                        if (velocity.linear.z < 0) velocity.linear.z = 0;
                    }
                }
            }
        }
    }
}

bool PhysicsSystem::check_block_collision(const Vector3& position, const Vector3& size) {
    if (!world_) return false;
    
    int min_x = static_cast<int>(std::floor(position.x - size.x / 2.0f));
    int max_x = static_cast<int>(std::floor(position.x + size.x / 2.0f));
    int min_y = static_cast<int>(std::floor(position.y));
    int max_y = static_cast<int>(std::floor(position.y + size.y));
    int min_z = static_cast<int>(std::floor(position.z - size.z / 2.0f));
    int max_z = static_cast<int>(std::floor(position.z + size.z / 2.0f));
    
    for (int x = min_x; x <= max_x; x++) {
        for (int y = min_y; y <= max_y; y++) {
            for (int z = min_z; z <= max_z; z++) {
                if (world_->get_block(x, y, z) != 0) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

} // namespace ecs
