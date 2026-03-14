#pragma once

#include "../system.hpp"
#include "../components.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/gl_mesh.hpp"

namespace voxel {
    class World;
}

namespace ecs {

class RAYFLOW_VOXEL_API RenderSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    
    /// Initialise default shader + mesh used for entities without explicit resources.
    /// Must be called once after GL context is ready.
    bool init();

    /// Release GPU resources.
    void shutdown();

    void set_world(voxel::World* world) { world_ = world; }

    /// Render 3D scene using the given camera.
    void render(entt::registry& registry, const rf::Camera& camera);

    void render_ui(entt::registry& registry, int screen_width, int screen_height);

    /// Access the built-in solid shader (other systems can borrow it).
    rf::GLShader& solidShader() { return solidShader_; }
    rf::GLMesh& defaultCube() { return defaultCube_; }
    
private:
    void render_crosshair(int screen_width, int screen_height);
    void render_player_info(entt::registry& registry);
    
    voxel::World* world_{nullptr};

    rf::GLShader solidShader_;
    rf::GLMesh   defaultCube_;
    bool         initialized_{false};
};

} // namespace ecs
