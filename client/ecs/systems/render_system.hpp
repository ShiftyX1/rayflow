#pragma once

#include "../system.hpp"
#include "../components.hpp"
#include <raylib.h>

// Forward declarations
namespace voxel {
    class World;
}

namespace ecs {

class RenderSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    
    void set_world(voxel::World* world) { world_ = world; }
    void render(entt::registry& registry, const Camera3D& camera);
    void render_ui(entt::registry& registry, int screen_width, int screen_height);
    
private:
    void render_crosshair(int screen_width, int screen_height);
    void render_player_info(entt::registry& registry);
    
    voxel::World* world_{nullptr};
};

} // namespace ecs
