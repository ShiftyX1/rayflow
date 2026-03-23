#pragma once

// =============================================================================
// Skybox — cubemap skybox renderer
//
// Phase 2: Replaced raylib Shader/Model/Texture2D with GLShader/GLMesh/GLTexture.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_shader.hpp"
#include "engine/renderer/gpu/gpu_texture.hpp"
#include "engine/renderer/gpu/gpu_mesh.hpp"
#include "engine/renderer/gpu/render_device.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/maps/rfmap_io.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace renderer {

class RAYFLOW_CLIENT_API Skybox {
public:
    static Skybox& instance();

    bool init(rf::RenderDevice& device);
    void shutdown();

    void set_kind(shared::maps::MapTemplate::SkyboxKind kind);
    shared::maps::MapTemplate::SkyboxKind kind() const { return kind_; }

    /// Draw the skybox using the given camera.
    void draw(const rf::Camera& camera);

    bool ready() const { return ready_; }

private:
    Skybox() = default;

    void ensure_cubemap_loaded_();
    const char* panorama_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind);
    const char* cubemap_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind);

    bool ready_{false};

    shared::maps::MapTemplate::SkyboxKind kind_{shared::maps::MapTemplate::SkyboxKind::Day};
    shared::maps::MapTemplate::SkyboxKind loaded_kind_{shared::maps::MapTemplate::SkyboxKind::None};

    std::string pano_path_;
    std::string cube_path_;

    rf::RenderDevice* device_{nullptr};
    std::unique_ptr<rf::IShader> shader_;
    std::unique_ptr<rf::IMesh> cubeMesh_;
    std::unique_ptr<rf::ITexture> cubemap_;
};

} // namespace renderer
