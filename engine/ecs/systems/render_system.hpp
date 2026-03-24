#pragma once

#include "../system.hpp"
#include "../components.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/renderer/gpu/gpu_shader.hpp"
#include "engine/renderer/gpu/gpu_mesh.hpp"
#include <memory>

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
    rf::IShader& solidShader() { return *solidShader_; }
    rf::IMesh& defaultCube() { return *defaultCube_; }
    
private:
    voxel::World* world_{nullptr};

    std::unique_ptr<rf::IShader> solidShader_;
    std::unique_ptr<rf::IMesh>   defaultCube_;
    bool         initialized_{false};
};

} // namespace ecs
