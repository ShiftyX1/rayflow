#pragma once

#include "../system.hpp"
#include "../components.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/camera.hpp"

namespace voxel {
    class World;
}

namespace ecs {

class RenderSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    
    void set_world(voxel::World* world) { world_ = world; }

    /// Render 3D scene using the given camera.
    void render(entt::registry& registry, const rf::Camera& camera);

    void render_ui(entt::registry& registry, int screen_width, int screen_height);
    
private:
    void render_crosshair(int screen_width, int screen_height);
    void render_player_info(entt::registry& registry);
    
    voxel::World* world_{nullptr};
};

} // namespace ecs
