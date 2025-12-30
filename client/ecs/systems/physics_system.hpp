#pragma once

#include "../system.hpp"
#include "../components.hpp"

namespace voxel {
    class World;
}

namespace ecs {

class PhysicsSystem : public System {
public:
    static constexpr float GRAVITY = 20.0f;
    
    void update(entt::registry& registry, float delta_time) override;
    void set_world(voxel::World* world) { world_ = world; }
    
private:
    void apply_gravity(entt::registry& registry, float delta_time);
    void apply_velocity(entt::registry& registry, float delta_time);
    bool check_block_collision(const Vector3& position, const Vector3& size);
    
    voxel::World* world_{nullptr};
};

} // namespace ecs
