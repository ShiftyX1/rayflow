#include "physics_system.hpp"
#include "engine/modules/voxel/client/world.hpp"
#include "engine/client/core/config.hpp"
#include <cmath>
#include <cstdio>

namespace ecs {

namespace {

bool should_log_collision_debug() {
    if (!core::Config::instance().logging().collision_debug) return false;

    static double last = 0.0;
    const double now = GetTime();
    if (now - last < 0.20) return false;
    last = now;
    return true;
}

} // namespace

void PhysicsSystem::update(entt::registry& registry, float delta_time) {
    apply_gravity(registry, delta_time);
    apply_velocity(registry, delta_time);
}

void PhysicsSystem::apply_gravity(entt::registry& registry, float delta_time) {
    auto view = registry.view<Velocity, GravityAffected, PlayerController>();
    
    for (auto entity : view) {
        auto& velocity = view.get<Velocity>(entity);
        auto& gravity = view.get<GravityAffected>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        if (!player.in_creative_mode && !player.on_ground) {
            velocity.linear.y -= GRAVITY * gravity.gravity_scale * delta_time;
        }
    }
}

void PhysicsSystem::apply_velocity(entt::registry& registry, float delta_time) {
    auto player_view = registry.view<Transform, Velocity, BoxCollider, PlayerController>();

    auto free_view = registry.view<Transform, Velocity>(entt::exclude<PlayerController>);

    for (auto entity : free_view) {
        auto& transform = free_view.get<Transform>(entity);
        auto& velocity = free_view.get<Velocity>(entity);

        transform.position.x += velocity.linear.x * delta_time;
        transform.position.y += velocity.linear.y * delta_time;
        transform.position.z += velocity.linear.z * delta_time;
    }

    if (!world_) {
        for (auto entity : player_view) {
            auto& transform = player_view.get<Transform>(entity);
            auto& velocity = player_view.get<Velocity>(entity);
            auto& player = player_view.get<PlayerController>(entity);

            player.on_ground = false;
            transform.position.x += velocity.linear.x * delta_time;
            transform.position.y += velocity.linear.y * delta_time;
            transform.position.z += velocity.linear.z * delta_time;
        }
        return;
    }

    constexpr float EPS = 1e-4f;
    constexpr float SKIN = 1e-3f;

    for (auto entity : player_view) {
        auto& transform = player_view.get<Transform>(entity);
        auto& velocity = player_view.get<Velocity>(entity);
        auto& collider = player_view.get<BoxCollider>(entity);
        auto& player = player_view.get<PlayerController>(entity);

        auto& prev = registry.get_or_emplace<PreviousPosition>(entity);
        prev.value = transform.position;
        prev.initialized = true;

        float half_w = collider.size.x / 2.0f;
        float height = collider.size.y;
        float half_d = collider.size.z / 2.0f;

        const bool was_on_ground = player.on_ground;
        player.on_ground = false;

        Vector3 pos = transform.position;

        auto resolve_x = [&](float dx) {
            if (dx == 0.0f) return;

            int min_y = static_cast<int>(std::floor(pos.y + EPS));
            int max_y = static_cast<int>(std::floor(pos.y + height - EPS));
            int min_z = static_cast<int>(std::floor(pos.z - half_d + EPS));
            int max_z = static_cast<int>(std::floor(pos.z + half_d - EPS));

            if (dx > 0.0f) {
                int check_x = static_cast<int>(std::floor((pos.x + half_w) - EPS));
                for (int by = min_y; by <= max_y; by++) {
                    for (int bz = min_z; bz <= max_z; bz++) {
                        if (world_->get_block(check_x, by, bz) != 0) {
                            pos.x = static_cast<float>(check_x) - half_w - SKIN;
                            velocity.linear.x = 0;
                            if (should_log_collision_debug()) {
                                TraceLog(LOG_DEBUG, "[phys] X clamp+ entity=%u check_x=%d by=%d bz=%d new_x=%.4f",
                                         static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                                         check_x, by, bz, pos.x);
                            }
                            return;
                        }
                    }
                }
            } else {
                int check_x = static_cast<int>(std::floor((pos.x - half_w) + EPS));
                for (int by = min_y; by <= max_y; by++) {
                    for (int bz = min_z; bz <= max_z; bz++) {
                        if (world_->get_block(check_x, by, bz) != 0) {
                            pos.x = static_cast<float>(check_x + 1) + half_w + SKIN;
                            velocity.linear.x = 0;
                            if (should_log_collision_debug()) {
                                TraceLog(LOG_DEBUG, "[phys] X clamp- entity=%u check_x=%d by=%d bz=%d new_x=%.4f",
                                         static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                                         check_x, by, bz, pos.x);
                            }
                            return;
                        }
                    }
                }
            }
        };

        auto resolve_z = [&](float dz) {
            if (dz == 0.0f) return;

            int min_y = static_cast<int>(std::floor(pos.y + EPS));
            int max_y = static_cast<int>(std::floor(pos.y + height - EPS));
            int min_x = static_cast<int>(std::floor(pos.x - half_w + EPS));
            int max_x = static_cast<int>(std::floor(pos.x + half_w - EPS));

            if (dz > 0.0f) {
                int check_z = static_cast<int>(std::floor((pos.z + half_d) - EPS));
                for (int by = min_y; by <= max_y; by++) {
                    for (int bx = min_x; bx <= max_x; bx++) {
                        if (world_->get_block(bx, by, check_z) != 0) {
                            pos.z = static_cast<float>(check_z) - half_d - SKIN;
                            velocity.linear.z = 0;
                            if (should_log_collision_debug()) {
                                TraceLog(LOG_DEBUG, "[phys] Z clamp+ entity=%u check_z=%d by=%d bx=%d new_z=%.4f",
                                         static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                                         check_z, by, bx, pos.z);
                            }
                            return;
                        }
                    }
                }
            } else {
                int check_z = static_cast<int>(std::floor((pos.z - half_d) + EPS));
                for (int by = min_y; by <= max_y; by++) {
                    for (int bx = min_x; bx <= max_x; bx++) {
                        if (world_->get_block(bx, by, check_z) != 0) {
                            pos.z = static_cast<float>(check_z + 1) + half_d + SKIN;
                            velocity.linear.z = 0;
                            if (should_log_collision_debug()) {
                                TraceLog(LOG_DEBUG, "[phys] Z clamp- entity=%u check_z=%d by=%d bx=%d new_z=%.4f",
                                         static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                                         check_z, by, bx, pos.z);
                            }
                            return;
                        }
                    }
                }
            }
        };

        auto resolve_y = [&](float dy) {
            float feet_y = pos.y;
            float head_y = pos.y + height;

            if (dy <= 0.0f) {
                int check_y = static_cast<int>(std::floor(feet_y - EPS));
                bool hit = false;
                for (int bx = static_cast<int>(std::floor(pos.x - half_w + EPS));
                     bx <= static_cast<int>(std::floor(pos.x + half_w - EPS)); bx++) {
                    for (int bz = static_cast<int>(std::floor(pos.z - half_d + EPS));
                         bz <= static_cast<int>(std::floor(pos.z + half_d - EPS)); bz++) {
                        if (world_->get_block(bx, check_y, bz) != 0) {
                            pos.y = static_cast<float>(check_y + 1);
                            velocity.linear.y = 0;
                            player.on_ground = true;
                            hit = true;
                            if (should_log_collision_debug()) {
                                TraceLog(LOG_DEBUG, "[phys] landed entity=%u check_y=%d bx=%d bz=%d new_y=%.4f",
                                         static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                                         check_y, bx, bz, pos.y);
                            }
                            break;
                        }
                    }
                    if (hit) break;
                }
            }

            if (dy > 0.0f) {
                int check_y = static_cast<int>(std::floor(head_y - EPS));
                for (int bx = static_cast<int>(std::floor(pos.x - half_w + EPS));
                     bx <= static_cast<int>(std::floor(pos.x + half_w - EPS)); bx++) {
                    for (int bz = static_cast<int>(std::floor(pos.z - half_d + EPS));
                         bz <= static_cast<int>(std::floor(pos.z + half_d - EPS)); bz++) {
                        if (world_->get_block(bx, check_y, bz) != 0) {
                            pos.y = static_cast<float>(check_y) - height;
                            velocity.linear.y = 0;
                            if (should_log_collision_debug()) {
                                TraceLog(LOG_DEBUG, "[phys] ceiling entity=%u check_y=%d bx=%d bz=%d new_y=%.4f",
                                         static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                                         check_y, bx, bz, pos.y);
                            }
                            return;
                        }
                    }
                }
            }
        };

        const float dx = velocity.linear.x * delta_time;
        if (dx != 0.0f) {
            pos.x += dx;
            resolve_x(dx);
        }

        const float dz = velocity.linear.z * delta_time;
        if (dz != 0.0f) {
            pos.z += dz;
            resolve_z(dz);
        }

        const float dy = velocity.linear.y * delta_time;
        pos.y += dy;
        resolve_y(dy);

        transform.position = pos;

        if (player.on_ground && !was_on_ground && should_log_collision_debug()) {
            TraceLog(LOG_DEBUG, "[phys] on_ground entity=%u pos=(%.4f, %.4f, %.4f)",
                     static_cast<unsigned>(static_cast<entt::id_type>(entity)),
                     transform.position.x, transform.position.y, transform.position.z);
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
