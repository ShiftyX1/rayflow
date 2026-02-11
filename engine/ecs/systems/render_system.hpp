#pragma once

#include "../system.hpp"
#include "../components.hpp"
#include "engine/core/math_types.hpp"

namespace voxel {
    class World;
}

// NOTE(migration): Camera3D placeholder — Phase 2 will replace with rf::Camera
struct Camera3D_Placeholder {};

namespace ecs {

class RenderSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    
    void set_world(voxel::World* world) { world_ = world; }
    // TODO(migration): Phase 2 will introduce proper camera type
    // void render(entt::registry& registry, const Camera3D& camera);
    void render_ui(entt::registry& registry, int screen_width, int screen_height);
    
private:
    void render_crosshair(int screen_width, int screen_height);
    void render_player_info(entt::registry& registry);
    
    voxel::World* world_{nullptr};
};

} // namespace ecs
