#pragma once

// BedWars-specific voxel physics system.
// Migrated from engine/ecs/systems/physics_system — game-specific because of
// tight coupling to voxel::World (Minecraft-style AABB vs block grid collision).

#include "engine/ecs/system.hpp"
#include "engine/ecs/components.hpp"

namespace voxel {
    class World;
}

namespace ecs {

class VoxelPhysicsSystem : public System {
public:
    static constexpr float GRAVITY = 20.0f;
    
    void update(entt::registry& registry, float delta_time) override;
    void set_world(voxel::World* world) { world_ = world; }
    
private:
    void apply_gravity(entt::registry& registry, float delta_time);
    void apply_velocity(entt::registry& registry, float delta_time);
    bool check_block_collision(const rf::Vec3& position, const rf::Vec3& size);
    
    voxel::World* world_{nullptr};
};

} // namespace ecs
