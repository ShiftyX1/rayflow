#include "skybox.hpp"

#include <rlgl.h>

#include <cstdio>

namespace renderer {

Skybox& Skybox::instance() {
    static Skybox g;
    return g;
}

bool Skybox::init() {
    if (ready_) return true;

    shader_ = LoadShader("shaders/skybox.vs", "shaders/skybox.fs");
    if (shader_.id == 0) {
        TraceLog(LOG_ERROR, "Skybox: failed to load shaders (shaders/skybox.*)");
        return false;
    }

    loc_env_map_ = GetShaderLocation(shader_, "environmentMap");

    // Simple cube mesh.
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    model_ = LoadModelFromMesh(cube);
    model_.materials[0].shader = shader_;
    has_model_ = (model_.meshCount > 0);

    ready_ = true;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;

    return true;
}

void Skybox::shutdown() {
    if (cubemap_.id != 0) {
        UnloadTexture(cubemap_);
        cubemap_ = {};
    }

    if (has_model_) {
        UnloadModel(model_);
        model_ = {};
        has_model_ = false;
    }

    if (shader_.id != 0) {
        UnloadShader(shader_);
        shader_ = {};
    }

    loc_env_map_ = -1;
    ready_ = false;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
}

void Skybox::set_kind(shared::maps::MapTemplate::SkyboxKind kind) {
    kind_ = kind;
}

const char* Skybox::panorama_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind) {
    const std::uint8_t id = static_cast<std::uint8_t>(kind);
    if (id == 0) return nullptr;

    // MV-1 originally defined 1=Day and 2=Night; we treat the numeric value as the panorama id:
    // Panorama_Sky_<id>-512x512.png (so existing maps remain stable).
    char buf[128];
    std::snprintf(buf, sizeof(buf), "textures/skybox/panorama/Panorama_Sky_%02u-512x512.png", static_cast<unsigned>(id));
    pano_path_ = buf;
    return pano_path_.c_str();
}

void Skybox::ensure_cubemap_loaded_() {
    if (!ready_) return;

    if (kind_ == loaded_kind_) {
        return;
    }

    if (cubemap_.id != 0) {
        UnloadTexture(cubemap_);
        cubemap_ = {};
    }

    loaded_kind_ = kind_;

    const char* pano_path = panorama_path_for_kind_(kind_);
    if (!pano_path) {
        return;
    }

    Image img = LoadImage(pano_path);
    if (img.data == nullptr) {
        TraceLog(LOG_WARNING, "Skybox: failed to load panorama image: %s", pano_path);
        return;
    }

    int layout = 0;
#ifdef CUBEMAP_LAYOUT_PANORAMA
    layout = CUBEMAP_LAYOUT_PANORAMA;
#else
    layout = CUBEMAP_LAYOUT_AUTO_DETECT;
#endif

    cubemap_ = LoadTextureCubemap(img, layout);
    UnloadImage(img);

    if (cubemap_.id == 0) {
        TraceLog(LOG_WARNING, "Skybox: failed to create cubemap from %s", pano_path);
        loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
        return;
    }

    SetTextureFilter(cubemap_, TEXTURE_FILTER_BILINEAR);
}

void Skybox::draw(const Camera3D& camera) {
    if (!ready_) return;
    if (!has_model_) return;
    if (kind_ == shared::maps::MapTemplate::SkyboxKind::None) return;

    ensure_cubemap_loaded_();
    if (cubemap_.id == 0) return;

    if (loc_env_map_ >= 0) {
        SetShaderValueTexture(shader_, loc_env_map_, cubemap_);
    }

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    // Center the skybox on camera so it doesn't appear to move.
    DrawModel(model_, camera.position, 50.0f, WHITE);

    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

} // namespace renderer
