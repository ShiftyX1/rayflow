#include "render_system.hpp"
#include "engine/modules/voxel/client/world.hpp"
#include "engine/core/math_types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace ecs {

void RenderSystem::update(entt::registry& registry, float delta_time) {
    (void)registry;
    (void)delta_time;
}

// NOTE(migration): render() commented out — Camera3D not yet defined.
// Phase 2 will reintroduce this with the new camera type.
#if 0
void RenderSystem::render(entt::registry& registry, const Camera3D& camera) {
    if (world_) {
        world_->render(camera);
    }
    
    auto mesh_view = registry.view<Transform, MeshComponent>();
    for (auto entity : mesh_view) {
        auto& transform = mesh_view.get<Transform>(entity);
        auto& mesh_comp = mesh_view.get<MeshComponent>(entity);
        
        auto model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(transform.position.x, transform.position.y, transform.position.z));
        // NOTE(migration): Phase 2 - replace with OpenGL mesh rendering
        // DrawMesh(mesh_comp.mesh, mesh_comp.material, model_matrix);
        (void)mesh_comp;
        (void)model_matrix;
    }
    
    auto model_view = registry.view<Transform, ModelComponent>();
    for (auto entity : model_view) {
        auto& transform = model_view.get<Transform>(entity);
        auto& model_comp = model_view.get<ModelComponent>(entity);
        
        if (model_comp.visible) {
            // NOTE(migration): Phase 2 - replace with OpenGL model rendering
            // DrawModel(model_comp.model, transform.position, 1.0f, rf::Color::White());
            (void)transform;
        }
    }
}
#endif

void RenderSystem::render_ui(entt::registry& registry, int screen_width, int screen_height) {
    (void)registry;
    render_crosshair(screen_width, screen_height);
}

void RenderSystem::render_crosshair(int screen_width, int screen_height) {
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;
    int size = 10;
    int thickness = 2;
    
    // NOTE(migration): Phase 3 - replace with OpenGL 2D quad rendering
    // DrawRectangle(center_x - size, center_y - thickness/2, size * 2, thickness, rf::Color::White());
    // DrawRectangle(center_x - thickness/2, center_y - size, thickness, size * 2, rf::Color::White());
    // DrawRectangleLines(center_x - size - 1, center_y - thickness/2 - 1,
    //                    size * 2 + 2, thickness + 2, rf::Color::Black());
    // DrawRectangleLines(center_x - thickness/2 - 1, center_y - size - 1,
    //                    thickness + 2, size * 2 + 2, rf::Color::Black());
    (void)center_x;
    (void)center_y;
    (void)size;
    (void)thickness;
}

void RenderSystem::render_player_info(entt::registry& registry) {
    auto view = registry.view<Transform, Velocity, PlayerController>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& velocity = view.get<Velocity>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        // NOTE(migration): Phase 3 - replace with engine text rendering
        // DrawText("Voxel Engine - ECS Architecture", 10, 10, 20, rf::Color::Black());
        // DrawText(TextFormat("Position: (%.1f, %.1f, %.1f)",
        //     transform.position.x, transform.position.y, transform.position.z),
        //     10, 40, 16, rf::Color{80,80,80,255});
        // DrawText(TextFormat("Velocity: (%.1f, %.1f, %.1f)",
        //     velocity.linear.x, velocity.linear.y, velocity.linear.z),
        //     10, 60, 16, rf::Color{80,80,80,255});
        // DrawText(TextFormat("On Ground: %s | Sprint: %s | Creative: %s",
        //     player.on_ground ? "YES" : "NO",
        //     player.is_sprinting ? "YES" : "NO",
        //     player.in_creative_mode ? "YES" : "NO"),
        //     10, 80, 16, rf::Color{80,80,80,255});
        // DrawFPS(10, 110);
        (void)transform;
        (void)velocity;
        (void)player;
    }
}

} // namespace ecs
