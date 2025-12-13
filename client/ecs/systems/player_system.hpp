#pragma once

#include "../system.hpp"
#include "../components.hpp"

// Forward declarations
namespace voxel {
    class World;
}

namespace ecs {

class PlayerSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    void set_world(voxel::World* world) { world_ = world; }

    // When true, PlayerSystem must not modify authoritative movement/physics state.
    // Client becomes a replica driven by server snapshots.
    void set_client_replica_mode(bool enabled) { clientReplicaMode_ = enabled; }
    
    // Create a player entity with all required components
    static entt::entity create_player(entt::registry& registry, const Vector3& spawn_position);
    
    // Get raylib Camera3D for rendering
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
