#pragma once

#include "../system.hpp"
#include "../components.hpp"

namespace voxel {
    class World;
}

namespace ecs {

class PlayerSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    void set_world(voxel::World* world) { world_ = world; }

    void set_client_replica_mode(bool enabled) { clientReplicaMode_ = enabled; }
    
    static entt::entity create_player(entt::registry& registry, const Vector3& spawn_position);
    
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
