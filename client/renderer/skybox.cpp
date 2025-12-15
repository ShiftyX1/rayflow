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

    // Hook into raylib's standard shader locations so DrawModel can bind the cubemap correctly.
    shader_.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader_, "mvp");
    shader_.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(shader_, "environmentMap");

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

const char* Skybox::cubemap_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind) {
    const std::uint8_t id = static_cast<std::uint8_t>(kind);
    if (id == 0) return nullptr;

    // Pre-baked cubemap cross layout (4x3): Cubemap_Sky_<id>-512x512.png
    // This is used as a fallback when panorama cubemap generation isn't supported by the raylib version.
    char buf[128];
    std::snprintf(buf, sizeof(buf), "textures/skybox/cubemap/Cubemap_Sky_%02u-512x512.png", static_cast<unsigned>(id));
    cube_path_ = buf;
    return cube_path_.c_str();
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

    // 1) Preferred path: panorama -> cubemap (only if supported by raylib).
    const char* pano_path = panorama_path_for_kind_(kind_);
#ifdef CUBEMAP_LAYOUT_PANORAMA
    if (pano_path) {
        Image img = LoadImage(pano_path);
        if (img.data == nullptr) {
            TraceLog(LOG_WARNING, "Skybox: failed to load panorama image: %s", pano_path);
        } else {
            cubemap_ = LoadTextureCubemap(img, CUBEMAP_LAYOUT_PANORAMA);
            UnloadImage(img);

            if (cubemap_.id != 0) {
                SetTextureFilter(cubemap_, TEXTURE_FILTER_BILINEAR);
                model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = cubemap_;
                return;
            }

            TraceLog(LOG_WARNING, "Skybox: failed to create cubemap from panorama %s", pano_path);
        }
    }
#endif

    // 2) Fallback: load pre-baked cubemap cross image (4x3). This works on older raylib versions.
    const char* cube_path = cubemap_path_for_kind_(kind_);
    if (!cube_path) {
        loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
        return;
    }

    Image cube_img = LoadImage(cube_path);
    if (cube_img.data == nullptr) {
        TraceLog(LOG_WARNING, "Skybox: failed to load cubemap image: %s", cube_path);
        if (pano_path) {
            TraceLog(LOG_WARNING, "Skybox: also could not use panorama %s (raylib lacks panorama cubemap support)", pano_path);
        }
        loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
        return;
    }

    cubemap_ = LoadTextureCubemap(cube_img, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);
    UnloadImage(cube_img);

    if (cubemap_.id == 0) {
        TraceLog(LOG_WARNING, "Skybox: failed to create cubemap from %s", cube_path);
        loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
        return;
    }

    SetTextureFilter(cubemap_, TEXTURE_FILTER_BILINEAR);
    model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = cubemap_;
}

void Skybox::draw(const Camera3D& camera) {
    if (!ready_) return;
    if (!has_model_) return;
    if (kind_ == shared::maps::MapTemplate::SkyboxKind::None) return;

    ensure_cubemap_loaded_();
    if (cubemap_.id == 0) return;

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    // Center the skybox on camera so it doesn't appear to move.
    DrawModel(model_, camera.position, 50.0f, WHITE);

    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

} // namespace renderer
