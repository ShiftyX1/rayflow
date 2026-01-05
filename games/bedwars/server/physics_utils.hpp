#pragma once

// BedWars Physics Utilities
// Collision detection and resolution helpers for player vs voxel world.

#include <engine/modules/voxel/shared/block.hpp>
#include <engine/modules/voxel/shared/block_state.hpp>
#include "voxel/terrain.hpp"

#include <cmath>
#include <cstdint>

namespace bedwars::server::physics {

// ============================================================================
// Constants
// ============================================================================

constexpr float kGravity = 20.0f;
constexpr float kJumpVelocity = 8.0f;
constexpr float kPlayerWidth = 0.6f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kWalkSpeed = 5.0f;
constexpr float kSprintSpeed = 8.0f;
constexpr float kMaxStepUpHeight = 0.5f;
constexpr float kEditorFlySpeed = 10.0f;

constexpr float kEps = 0.001f;
constexpr float kSkin = 0.001f;
constexpr float kDegToRad = 3.14159265f / 180.0f;

// ============================================================================
// Helpers
// ============================================================================

inline int fast_floor(float v) {
    return static_cast<int>(std::floor(v));
}

// ============================================================================
// AABB Collision
// ============================================================================

/// Check if player AABB collides with a block's collision box.
inline bool check_aabb_collision(
    const shared::voxel::BlockCollisionInfo& coll,
    int bx, int by, int bz,
    float px, float py, float pz,
    float half_w, float height, float half_d
) {
    if (!coll.hasCollision) return false;
    
    float block_min_x = static_cast<float>(bx) + coll.minX;
    float block_max_x = static_cast<float>(bx) + coll.maxX;
    float block_min_y = static_cast<float>(by) + coll.minY;
    float block_max_y = static_cast<float>(by) + coll.maxY;
    float block_min_z = static_cast<float>(bz) + coll.minZ;
    float block_max_z = static_cast<float>(bz) + coll.maxZ;
    
    float player_min_x = px - half_w;
    float player_max_x = px + half_w;
    float player_min_y = py;
    float player_max_y = py + height;
    float player_min_z = pz - half_d;
    float player_max_z = pz + half_d;
    
    return player_min_x < block_max_x && player_max_x > block_min_x &&
           player_min_y < block_max_y && player_max_y > block_min_y &&
           player_min_z < block_max_z && player_max_z > block_min_z;
}

/// Check collision with a specific block type.
inline bool check_block_collision_3d(
    shared::voxel::BlockType block_type,
    int bx, int by, int bz,
    float px, float py, float pz,
    float half_w, float height, float half_d
) {
    auto coll = shared::voxel::get_collision_info(block_type);
    return check_aabb_collision(coll, bx, by, bz, px, py, pz, half_w, height, half_d);
}

/// Check collision with block type + state (for fences, slabs, etc.)
inline bool check_block_collision_3d_with_state(
    shared::voxel::BlockType block_type,
    shared::voxel::BlockRuntimeState state,
    int bx, int by, int bz,
    float px, float py, float pz,
    float half_w, float height, float half_d
) {
    shared::voxel::BlockCollisionInfo boxes[5];
    int count = shared::voxel::get_collision_boxes(block_type, state, boxes, 5);
    
    for (int i = 0; i < count; ++i) {
        if (check_aabb_collision(boxes[i], bx, by, bz, px, py, pz, half_w, height, half_d)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Step-Up Logic
// ============================================================================

/// Get the maximum step-up height at player's feet.
inline float get_obstacle_step_height(
    const ::bedwars::voxel::Terrain& terrain,
    float px, float py, float pz,
    float half_w, float half_d
) {
    int feet_y = fast_floor(py);
    float max_step_height = 0.0f;
    
    for (int bx = fast_floor(px - half_w + kEps); bx <= fast_floor(px + half_w - kEps); ++bx) {
        for (int bz = fast_floor(pz - half_d + kEps); bz <= fast_floor(pz + half_d - kEps); ++bz) {
            auto block_type = terrain.get_block(bx, feet_y, bz);
            auto coll = shared::voxel::get_collision_info(block_type);
            if (!coll.hasCollision) continue;
            
            float block_min_x = static_cast<float>(bx) + coll.minX;
            float block_max_x = static_cast<float>(bx) + coll.maxX;
            float block_min_z = static_cast<float>(bz) + coll.minZ;
            float block_max_z = static_cast<float>(bz) + coll.maxZ;
            
            if (px - half_w < block_max_x && px + half_w > block_min_x &&
                pz - half_d < block_max_z && pz + half_d > block_min_z) {
                float ground_height = static_cast<float>(feet_y) + std::min(coll.maxY, 1.0f);
                float step_height = ground_height - py;
                if (step_height > max_step_height && step_height > 0.0f) {
                    max_step_height = step_height;
                }
            }
        }
    }
    
    return max_step_height;
}

/// Try to step up over an obstacle. Returns true if step-up succeeded.
inline bool try_step_up(
    const ::bedwars::voxel::Terrain& terrain,
    float px, float& py, float pz,
    float half_w, float height, float half_d
) {
    float step_height = get_obstacle_step_height(terrain, px, py, pz, half_w, half_d);
    
    if (step_height <= 0.0f || step_height > kMaxStepUpHeight) {
        return false;
    }
    
    float new_y = py + step_height + kSkin;
    int head_y = fast_floor(new_y + height - kEps);
    
    // Check headroom
    for (int bx = fast_floor(px - half_w + kEps); bx <= fast_floor(px + half_w - kEps); ++bx) {
        for (int bz = fast_floor(pz - half_d + kEps); bz <= fast_floor(pz + half_d - kEps); ++bz) {
            auto block_type = terrain.get_block(bx, head_y, bz);
            if (check_block_collision_3d(block_type, bx, head_y, bz, px, new_y, pz, half_w, height, half_d)) {
                return false; // No headroom
            }
        }
    }
    
    py = new_y;
    return true;
}

// ============================================================================
// Axis-aligned Collision Resolution
// ============================================================================

/// Resolve X-axis collision. Modifies px and vx if collision found.
inline void resolve_voxel_x(
    const ::bedwars::voxel::Terrain& terrain,
    float& px, float py, float pz,
    float& vx, float dx
) {
    if (dx == 0.0f) return;

    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    int min_y = fast_floor(py + kEps) - 1;
    if (min_y < 0) min_y = 0;
    int max_y = fast_floor(py + height - kEps);
    int min_z = fast_floor(pz - half_d + kEps);
    int max_z = fast_floor(pz + half_d - kEps);

    if (dx > 0.0f) {
        int check_x = fast_floor((px + half_w) - kEps);
        for (int by = min_y; by <= max_y; ++by) {
            for (int bz = min_z; bz <= max_z; ++bz) {
                auto block_type = terrain.get_block(check_x, by, bz);
                auto block_state = terrain.get_block_state(check_x, by, bz);
                if (check_block_collision_3d_with_state(block_type, block_state, check_x, by, bz, 
                                                        px, py, pz, half_w, height, half_d)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(check_x) + 1.0f;
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], check_x, by, bz, px, py, pz, half_w, height, half_d)) {
                            float edge = static_cast<float>(check_x) + boxes[i].minX;
                            if (edge < block_edge) block_edge = edge;
                        }
                    }
                    px = block_edge - half_w - kSkin;
                    vx = 0.0f;
                    return;
                }
            }
        }
    } else {
        int check_x = fast_floor((px - half_w) + kEps);
        for (int by = min_y; by <= max_y; ++by) {
            for (int bz = min_z; bz <= max_z; ++bz) {
                auto block_type = terrain.get_block(check_x, by, bz);
                auto block_state = terrain.get_block_state(check_x, by, bz);
                if (check_block_collision_3d_with_state(block_type, block_state, check_x, by, bz,
                                                        px, py, pz, half_w, height, half_d)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(check_x);
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], check_x, by, bz, px, py, pz, half_w, height, half_d)) {
                            float edge = static_cast<float>(check_x) + boxes[i].maxX;
                            if (edge > block_edge) block_edge = edge;
                        }
                    }
                    px = block_edge + half_w + kSkin;
                    vx = 0.0f;
                    return;
                }
            }
        }
    }
}

/// Resolve Z-axis collision. Modifies pz and vz if collision found.
inline void resolve_voxel_z(
    const ::bedwars::voxel::Terrain& terrain,
    float px, float py, float& pz,
    float& vz, float dz
) {
    if (dz == 0.0f) return;

    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    int min_y = fast_floor(py + kEps) - 1;
    if (min_y < 0) min_y = 0;
    int max_y = fast_floor(py + height - kEps);
    int min_x = fast_floor(px - half_w + kEps);
    int max_x = fast_floor(px + half_w - kEps);

    if (dz > 0.0f) {
        int check_z = fast_floor((pz + half_d) - kEps);
        for (int by = min_y; by <= max_y; ++by) {
            for (int bx = min_x; bx <= max_x; ++bx) {
                auto block_type = terrain.get_block(bx, by, check_z);
                auto block_state = terrain.get_block_state(bx, by, check_z);
                if (check_block_collision_3d_with_state(block_type, block_state, bx, by, check_z,
                                                        px, py, pz, half_w, height, half_d)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(check_z) + 1.0f;
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], bx, by, check_z, px, py, pz, half_w, height, half_d)) {
                            float edge = static_cast<float>(check_z) + boxes[i].minZ;
                            if (edge < block_edge) block_edge = edge;
                        }
                    }
                    pz = block_edge - half_d - kSkin;
                    vz = 0.0f;
                    return;
                }
            }
        }
    } else {
        int check_z = fast_floor((pz - half_d) + kEps);
        for (int by = min_y; by <= max_y; ++by) {
            for (int bx = min_x; bx <= max_x; ++bx) {
                auto block_type = terrain.get_block(bx, by, check_z);
                auto block_state = terrain.get_block_state(bx, by, check_z);
                if (check_block_collision_3d_with_state(block_type, block_state, bx, by, check_z,
                                                        px, py, pz, half_w, height, half_d)) {
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    float block_edge = static_cast<float>(check_z);
                    for (int i = 0; i < count; ++i) {
                        if (check_aabb_collision(boxes[i], bx, by, check_z, px, py, pz, half_w, height, half_d)) {
                            float edge = static_cast<float>(check_z) + boxes[i].maxZ;
                            if (edge > block_edge) block_edge = edge;
                        }
                    }
                    pz = block_edge + half_d + kSkin;
                    vz = 0.0f;
                    return;
                }
            }
        }
    }
}

/// Resolve Y-axis collision. Modifies py, vy, and onGround.
inline void resolve_voxel_y(
    const ::bedwars::voxel::Terrain& terrain,
    float px, float& py, float pz,
    float& vy, float dy,
    bool& onGround
) {
    const float half_w = kPlayerWidth * 0.5f;
    const float height = kPlayerHeight;
    const float half_d = kPlayerWidth * 0.5f;

    // Falling down - check for ground
    if (dy <= 0.0f) {
        for (int check_y = fast_floor(py - kEps); check_y >= fast_floor(py - 1.0f); --check_y) {
            for (int bx = fast_floor(px - half_w + kEps); bx <= fast_floor(px + half_w - kEps); ++bx) {
                for (int bz = fast_floor(pz - half_d + kEps); bz <= fast_floor(pz + half_d - kEps); ++bz) {
                    auto block_type = terrain.get_block(bx, check_y, bz);
                    auto block_state = terrain.get_block_state(bx, check_y, bz);
                    
                    shared::voxel::BlockCollisionInfo boxes[5];
                    int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                    if (count == 0) continue;
                    
                    float best_ground_height = -9999.0f;
                    bool found_xz_overlap = false;
                    
                    for (int i = 0; i < count; ++i) {
                        const auto& coll = boxes[i];
                        if (!coll.hasCollision) continue;
                        
                        float block_min_x = static_cast<float>(bx) + coll.minX;
                        float block_max_x = static_cast<float>(bx) + coll.maxX;
                        float block_min_z = static_cast<float>(bz) + coll.minZ;
                        float block_max_z = static_cast<float>(bz) + coll.maxZ;
                        
                        if (!(px - half_w < block_max_x && px + half_w > block_min_x &&
                              pz - half_d < block_max_z && pz + half_d > block_min_z)) {
                            continue;
                        }
                        
                        found_xz_overlap = true;
                        float ground_height = static_cast<float>(check_y) + coll.maxY;
                        if (ground_height > best_ground_height) {
                            best_ground_height = ground_height;
                        }
                    }
                    
                    if (!found_xz_overlap) continue;
                    
                    if (py <= best_ground_height + kEps && py > best_ground_height - 0.5f) {
                        py = best_ground_height;
                        if (vy < 0.0f) vy = 0.0f;
                        onGround = true;
                        return;
                    }
                }
            }
        }
    }

    // Rising up - check for ceiling
    if (dy > 0.0f) {
        int check_y = fast_floor((py + height) - kEps);
        for (int bx = fast_floor(px - half_w + kEps); bx <= fast_floor(px + half_w - kEps); ++bx) {
            for (int bz = fast_floor(pz - half_d + kEps); bz <= fast_floor(pz + half_d - kEps); ++bz) {
                auto block_type = terrain.get_block(bx, check_y, bz);
                auto block_state = terrain.get_block_state(bx, check_y, bz);
                
                shared::voxel::BlockCollisionInfo boxes[5];
                int count = shared::voxel::get_collision_boxes(block_type, block_state, boxes, 5);
                if (count == 0) continue;
                
                float best_ceiling = 9999.0f;
                bool found_xz_overlap = false;
                
                for (int i = 0; i < count; ++i) {
                    const auto& coll = boxes[i];
                    if (!coll.hasCollision) continue;
                    
                    float block_min_x = static_cast<float>(bx) + coll.minX;
                    float block_max_x = static_cast<float>(bx) + coll.maxX;
                    float block_min_z = static_cast<float>(bz) + coll.minZ;
                    float block_max_z = static_cast<float>(bz) + coll.maxZ;
                    
                    if (!(px - half_w < block_max_x && px + half_w > block_min_x &&
                          pz - half_d < block_max_z && pz + half_d > block_min_z)) {
                        continue;
                    }
                    
                    found_xz_overlap = true;
                    float block_bottom = static_cast<float>(check_y) + coll.minY;
                    if (block_bottom < best_ceiling) {
                        best_ceiling = block_bottom;
                    }
                }
                
                if (!found_xz_overlap) continue;
                
                if (py + height > best_ceiling) {
                    py = best_ceiling - height;
                    if (vy > 0.0f) vy = 0.0f;
                    return;
                }
            }
        }
    }
}

// ============================================================================
// Full Physics Simulation Step
// ============================================================================

/// Simulate one physics step with full collision resolution and step-up.
inline void simulate_physics_step(
    const ::bedwars::voxel::Terrain& terrain,
    float& px, float& py, float& pz,
    float& vx, float& vy, float& vz,
    bool& onGround, bool& lastJumpHeld,
    float moveX, float moveY, float yaw,
    bool jumpHeld, bool sprinting,
    float dt
) {
    const float half_w = kPlayerWidth * 0.5f;
    const float half_d = kPlayerWidth * 0.5f;
    
    const float speed = sprinting ? kSprintSpeed : kWalkSpeed;
    
    // Calculate movement direction from yaw
    const float yawRad = yaw * kDegToRad;
    const float forwardX = std::sin(yawRad);
    const float forwardZ = std::cos(yawRad);
    const float rightX = std::cos(yawRad);
    const float rightZ = -std::sin(yawRad);
    
    // Apply input to velocity
    vx = rightX * moveX * speed + forwardX * moveY * speed;
    vz = rightZ * moveX * speed + forwardZ * moveY * speed;
    
    // Jump
    const bool jumpPressed = jumpHeld && !lastJumpHeld;
    lastJumpHeld = jumpHeld;
    
    if (onGround && jumpPressed) {
        vy = kJumpVelocity;
        onGround = false;
    }
    
    // Gravity
    if (!onGround) {
        vy -= kGravity * dt;
    } else if (vy < 0.0f) {
        vy = 0.0f;
    }
    
    // Move X with step-up
    float dx = vx * dt;
    if (dx != 0.0f) {
        float old_px = px;
        px += dx;
        resolve_voxel_x(terrain, px, py, pz, vx, dx);
        
        // If blocked and on ground, try step-up
        if (onGround && px == old_px && vx == 0.0f) {
            px = old_px + dx;
            if (try_step_up(terrain, px, py, pz, half_w, kPlayerHeight, half_d)) {
                // Step-up succeeded
            } else {
                px = old_px;
                vx = 0.0f;
            }
        }
    }
    
    // Move Z with step-up
    float dz = vz * dt;
    if (dz != 0.0f) {
        float old_pz = pz;
        pz += dz;
        resolve_voxel_z(terrain, px, py, pz, vz, dz);
        
        // If blocked and on ground, try step-up
        if (onGround && pz == old_pz && vz == 0.0f) {
            pz = old_pz + dz;
            if (try_step_up(terrain, px, py, pz, half_w, kPlayerHeight, half_d)) {
                // Step-up succeeded
            } else {
                pz = old_pz;
                vz = 0.0f;
            }
        }
    }
    
    // Move Y
    float dy = vy * dt;
    py += dy;
    onGround = false;
    resolve_voxel_y(terrain, px, py, pz, vy, dy, onGround);
    
    // Safety floor
    if (py < 0.0f) {
        py = 80.0f;
        px = 0.0f;
        pz = 0.0f;
        vy = 0.0f;
    }
}

} // namespace bedwars::server::physics
