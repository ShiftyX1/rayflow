#pragma once

#include "engine/core/math_types.hpp"

#include "engine/maps/rfmap_io.hpp"

#include <cstdint>
#include <string>

namespace renderer {

class Skybox {
public:
    static Skybox& instance();

    bool init();
    void shutdown();

    void set_kind(shared::maps::MapTemplate::SkyboxKind kind);
    shared::maps::MapTemplate::SkyboxKind kind() const { return kind_; }

    // NOTE(migration): draw() needs a camera. Phase 2 will define rf::Camera.
    // void draw(const Camera3D& camera);
    void draw_stub() {} // placeholder until Phase 2

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

    // NOTE(migration): Shader/Model/Texture2D are raylib types.
    // Phase 2 will replace with GLShader/GLMesh/GLTexture.
    struct ShaderPlaceholder {};
    struct ModelPlaceholder {};
    struct Texture2DPlaceholder {};

    ShaderPlaceholder shader_{};

    ModelPlaceholder model_{};
    bool has_model_{false};

    Texture2DPlaceholder cubemap_{};
};

} // namespace renderer
