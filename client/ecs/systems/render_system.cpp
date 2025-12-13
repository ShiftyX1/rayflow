#include "render_system.hpp"
#include "../../voxel/world.hpp"
#include <raymath.h>

namespace ecs {

void RenderSystem::update(entt::registry& registry, float delta_time) {
    // This system doesn't update state, it's called separately for rendering
    (void)registry;
    (void)delta_time;
}

void RenderSystem::render(entt::registry& registry, const Camera3D& camera) {
    // Render voxel world
    if (world_) {
        world_->render(camera);
    }
    
    // Render meshes
    auto mesh_view = registry.view<Transform, MeshComponent>();
    for (auto entity : mesh_view) {
        auto& transform = mesh_view.get<Transform>(entity);
        auto& mesh_comp = mesh_view.get<MeshComponent>(entity);
        
        DrawMesh(mesh_comp.mesh, mesh_comp.material, 
                 MatrixTranslate(transform.position.x, transform.position.y, transform.position.z));
    }
    
    // Render models
    auto model_view = registry.view<Transform, ModelComponent>();
    for (auto entity : model_view) {
        auto& transform = model_view.get<Transform>(entity);
        auto& model_comp = model_view.get<ModelComponent>(entity);
        
        if (model_comp.visible) {
            DrawModel(model_comp.model, transform.position, 1.0f, WHITE);
        }
    }
}

void RenderSystem::render_ui(entt::registry& registry, int screen_width, int screen_height) {
    (void)registry;
    render_crosshair(screen_width, screen_height);
}

void RenderSystem::render_crosshair(int screen_width, int screen_height) {
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;
    int size = 10;
    int thickness = 2;
    
    // Draw crosshair
    DrawRectangle(center_x - size, center_y - thickness/2, size * 2, thickness, WHITE);
    DrawRectangle(center_x - thickness/2, center_y - size, thickness, size * 2, WHITE);
    
    // Draw outline for visibility
    DrawRectangleLines(center_x - size - 1, center_y - thickness/2 - 1, 
                       size * 2 + 2, thickness + 2, BLACK);
    DrawRectangleLines(center_x - thickness/2 - 1, center_y - size - 1, 
                       thickness + 2, size * 2 + 2, BLACK);
}

void RenderSystem::render_player_info(entt::registry& registry) {
    auto view = registry.view<Transform, Velocity, PlayerController>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& velocity = view.get<Velocity>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        DrawText("Voxel Engine - ECS Architecture", 10, 10, 20, BLACK);
        
        DrawText(TextFormat("Position: (%.1f, %.1f, %.1f)", 
            transform.position.x, transform.position.y, transform.position.z), 
            10, 40, 16, DARKGRAY);
        
        DrawText(TextFormat("Velocity: (%.1f, %.1f, %.1f)", 
            velocity.linear.x, velocity.linear.y, velocity.linear.z), 
            10, 60, 16, DARKGRAY);
        
        DrawText(TextFormat("On Ground: %s | Sprint: %s | Creative: %s", 
            player.on_ground ? "YES" : "NO",
            player.is_sprinting ? "YES" : "NO",
            player.in_creative_mode ? "YES" : "NO"), 
            10, 80, 16, DARKGRAY);
        
        DrawFPS(10, 110);
    }
}

} // namespace ecs
