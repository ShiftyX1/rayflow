#include "render_system.hpp"
#include "engine/modules/voxel/client/world.hpp"
#include "engine/core/math_types.hpp"
#include "engine/core/logging.hpp"
#include "engine/renderer/batch_2d.hpp"
#include "engine/renderer/gl_font.hpp"
#include "engine/renderer/gpu/gpu_shader.hpp"
#include "engine/renderer/gpu/gpu_mesh.hpp"
#include "engine/client/core/resources.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

namespace ecs {

// ============================================================================
// Init / Shutdown
// ============================================================================

bool RenderSystem::init() {
    if (initialized_) return true;

    solidShader_ = resources::load_shader("shaders/solid.vs", "shaders/solid.fs");
    if (!solidShader_ || !solidShader_->isValid()) {
        TraceLog(LOG_ERROR, "RenderSystem: failed to load solid shader");
        return false;
    }

    defaultCube_ = resources::create_cube(1.0f);
    if (!defaultCube_ || !defaultCube_->isValid()) {
        TraceLog(LOG_ERROR, "RenderSystem: failed to create default cube mesh");
        return false;
    }

    initialized_ = true;
    TraceLog(LOG_INFO, "RenderSystem initialized (solid shader + default cube)");
    return true;
}

void RenderSystem::shutdown() {
    solidShader_.reset();
    defaultCube_.reset();
    initialized_ = false;
}

// ============================================================================
// Update
// ============================================================================

void RenderSystem::update(entt::registry& registry, float delta_time) {
    (void)registry;
    (void)delta_time;
}

// ============================================================================
// Render 3D entities
// ============================================================================

void RenderSystem::render(entt::registry& registry, const rf::Camera& camera) {
    if (world_) {
        world_->render(camera);
    }

    if (!initialized_) return;

    rf::Mat4 vp = camera.viewProjectionMatrix();

    // --- MeshComponent entities ---
    {
        auto mesh_view = registry.view<Transform, MeshComponent>();
        for (auto entity : mesh_view) {
            auto& transform = mesh_view.get<Transform>(entity);
            auto& mesh_comp = mesh_view.get<MeshComponent>(entity);

            if (!mesh_comp.visible) continue;

            // Pick shader & mesh (use defaults if not set)
            rf::IShader& shader = mesh_comp.shader ? *mesh_comp.shader : *solidShader_;
            rf::IMesh&   mesh   = mesh_comp.mesh   ? *mesh_comp.mesh   : *defaultCube_;

            rf::Mat4 model = glm::translate(rf::Mat4(1.0f), transform.position)
                           * glm::scale(rf::Mat4(1.0f), transform.scale);
            rf::Mat4 mvp   = vp * model;

            shader.bind();
            shader.setMat4("mvp", mvp);
            shader.setVec4("color", mesh_comp.color.normalized());
            mesh.draw();
        }
    }

    // --- ModelComponent entities ---
    {
        auto model_view = registry.view<Transform, ModelComponent>();
        for (auto entity : model_view) {
            auto& transform = model_view.get<Transform>(entity);
            auto& model_comp = model_view.get<ModelComponent>(entity);

            if (!model_comp.visible) continue;

            rf::IShader& shader = model_comp.shader ? *model_comp.shader : *solidShader_;
            rf::IMesh&   mesh   = model_comp.mesh   ? *model_comp.mesh   : *defaultCube_;

            rf::Mat4 model = glm::translate(rf::Mat4(1.0f), transform.position)
                           * glm::scale(rf::Mat4(1.0f), transform.scale);
            rf::Mat4 mvp   = vp * model;

            shader.bind();
            shader.setMat4("mvp", mvp);
            shader.setVec4("color", model_comp.color.normalized());
            mesh.draw();
        }
    }

    solidShader_->unbind();
}

void RenderSystem::render_ui(entt::registry& registry, int screen_width, int screen_height) {
    (void)registry;
    (void)screen_width;
    (void)screen_height;
}

} // namespace ecs
