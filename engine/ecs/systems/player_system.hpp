#pragma once

#include "../system.hpp"
#include "../components.hpp"
#include "engine/core/math_types.hpp"

namespace voxel {
    class World;
}

namespace ecs {

// NOTE(migration): Phase 0 placeholder — will be replaced by a proper camera struct.
struct Camera3D {
    rf::Vec3 position{0.f, 0.f, 0.f};
    rf::Vec3 target{0.f, 0.f, 0.f};
    rf::Vec3 up{0.f, 1.f, 0.f};
    float fovy{60.f};
    int projection{0};
};

class PlayerSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    void set_world(voxel::World* world) { world_ = world; }

    void set_client_replica_mode(bool enabled) { clientReplicaMode_ = enabled; }
    
    static entt::entity create_player(entt::registry& registry, const rf::Vec3& spawn_position);
    
    static Camera3D get_camera(entt::registry& registry, entt::entity player);
    
private:
    void handle_movement(entt::registry& registry, float delta_time);
    void handle_jumping(entt::registry& registry);
    void handle_creative_mode(entt::registry& registry);
    void handle_tool_selection(entt::registry& registry);
    
    voxel::World* world_{nullptr};
    bool clientReplicaMode_{false};
};

} // namespace ecs
