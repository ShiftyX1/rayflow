#include "render_system.hpp"
#include "engine/modules/voxel/client/world.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/batch_2d.hpp"
#include "engine/renderer/gl_font.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

namespace ecs {

void RenderSystem::update(entt::registry& registry, float delta_time) {
    (void)registry;
    (void)delta_time;
}

void RenderSystem::render(entt::registry& registry, const rf::Camera& camera) {
    if (world_) {
        world_->render(camera);
    }

    // Entity mesh rendering (Phase 5 — proper material pipeline)
    auto mesh_view = registry.view<Transform, MeshComponent>();
    for (auto entity : mesh_view) {
        auto& transform = mesh_view.get<Transform>(entity);
        auto& mesh_comp = mesh_view.get<MeshComponent>(entity);

        if (!mesh_comp.visible) continue;

        // TODO(Phase 5): Bind material shader, upload model matrix, draw mesh
        (void)transform;
    }

    auto model_view = registry.view<Transform, ModelComponent>();
    for (auto entity : model_view) {
        auto& transform = model_view.get<Transform>(entity);
        auto& model_comp = model_view.get<ModelComponent>(entity);

        if (!model_comp.visible) continue;

        // TODO(Phase 5): Bind model shader, draw model
        (void)transform;
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
    
    auto& batch = rf::Batch2D::instance();
    // Horizontal bar
    batch.drawRect(static_cast<float>(center_x - size),
                   static_cast<float>(center_y - thickness / 2),
                   static_cast<float>(size * 2),
                   static_cast<float>(thickness), rf::Color::White());
    // Vertical bar
    batch.drawRect(static_cast<float>(center_x - thickness / 2),
                   static_cast<float>(center_y - size),
                   static_cast<float>(thickness),
                   static_cast<float>(size * 2), rf::Color::White());
    // Outlines
    batch.drawRectLines(static_cast<float>(center_x - size - 1),
                        static_cast<float>(center_y - thickness / 2 - 1),
                        static_cast<float>(size * 2 + 2),
                        static_cast<float>(thickness + 2), rf::Color::Black());
    batch.drawRectLines(static_cast<float>(center_x - thickness / 2 - 1),
                        static_cast<float>(center_y - size - 1),
                        static_cast<float>(thickness + 2),
                        static_cast<float>(size * 2 + 2), rf::Color::Black());
}

void RenderSystem::render_player_info(entt::registry& registry) {
    auto view = registry.view<Transform, Velocity, PlayerController>();
    
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& velocity = view.get<Velocity>(entity);
        auto& player = view.get<PlayerController>(entity);
        
        auto& batch = rf::Batch2D::instance();
        
        batch.drawText("Voxel Engine - ECS Architecture", 10, 10, 20, rf::Color::Black());
        
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Position: (%.1f, %.1f, %.1f)",
            transform.position.x, transform.position.y, transform.position.z);
        batch.drawText(buf, 10, 40, 16, rf::Color{80,80,80,255});
        
        std::snprintf(buf, sizeof(buf), "Velocity: (%.1f, %.1f, %.1f)",
            velocity.linear.x, velocity.linear.y, velocity.linear.z);
        batch.drawText(buf, 10, 60, 16, rf::Color{80,80,80,255});
        
        std::snprintf(buf, sizeof(buf), "On Ground: %s | Sprint: %s | Creative: %s",
            player.on_ground ? "YES" : "NO",
            player.is_sprinting ? "YES" : "NO",
            player.in_creative_mode ? "YES" : "NO");
        batch.drawText(buf, 10, 80, 16, rf::Color{80,80,80,255});
    }
}

} // namespace ecs
