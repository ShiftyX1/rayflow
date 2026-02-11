#include "skybox.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"

#include <cstdio>

namespace renderer {

Skybox& Skybox::instance() {
    static Skybox g;
    return g;
}

bool Skybox::init() {
    // TODO(Phase 2): Re-implement with GLFW+OpenGL shader/mesh loading.
    // if (ready_) return true;
    //
    // shader_ = resources::load_shader("shaders/skybox.vs", "shaders/skybox.fs");
    // if (shader_.id == 0) {
    //     TraceLog(LOG_ERROR, "Skybox: failed to load shaders (shaders/skybox.*)");
    //     return false;
    // }
    //
    // shader_.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader_, "mvp");
    // shader_.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(shader_, "environmentMap");
    //
    // Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    // model_ = LoadModelFromMesh(cube);
    // model_.materials[0].shader = shader_;
    // has_model_ = (model_.meshCount > 0);

    ready_ = false;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;

    return false;
}

void Skybox::shutdown() {
    // TODO(Phase 2): Re-implement resource cleanup with OpenGL.
    // if (cubemap_.id != 0) {
    //     UnloadTexture(cubemap_);
    //     cubemap_ = {};
    // }
    //
    // if (has_model_) {
    //     UnloadModel(model_);
    //     model_ = {};
    // }
    //
    // if (shader_.id != 0) {
    //     UnloadShader(shader_);
    //     shader_ = {};
    // }

    has_model_ = false;
    shader_ = {};
    model_ = {};
    cubemap_ = {};
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
    // TODO(Phase 2): Re-implement cubemap loading with OpenGL.
    if (!ready_) return;
    if (kind_ == loaded_kind_) return;

    // const char* pano_path = panorama_path_for_kind_(kind_);
    // #ifdef CUBEMAP_LAYOUT_PANORAMA
    //     if (pano_path) {
    //         Image img = resources::load_image(pano_path);
    //         ...
    //         cubemap_ = LoadTextureCubemap(img, CUBEMAP_LAYOUT_PANORAMA);
    //         UnloadImage(img);
    //         SetTextureFilter(cubemap_, TEXTURE_FILTER_BILINEAR);
    //         model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = cubemap_;
    //     }
    // #endif
    //
    // const char* cube_path = cubemap_path_for_kind_(kind_);
    // Image cube_img = resources::load_image(cube_path);
    // cubemap_ = LoadTextureCubemap(cube_img, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);
    // UnloadImage(cube_img);
    // SetTextureFilter(cubemap_, TEXTURE_FILTER_BILINEAR);
    // model_.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = cubemap_;

    loaded_kind_ = kind_;
}

// NOTE(migration): draw(const Camera3D&) removed — header now exposes draw_stub().
// TODO(Phase 4): Re-implement skybox rendering with raw OpenGL draw calls.
// void Skybox::draw(const Camera3D& camera) {
//     if (!ready_) return;
//     if (!has_model_) return;
//     if (kind_ == shared::maps::MapTemplate::SkyboxKind::None) return;
//
//     ensure_cubemap_loaded_();
//     if (cubemap_.id == 0) return;
//
//     rlDisableBackfaceCulling();
//     rlDisableDepthMask();
//
//     DrawModel(model_, camera.position, 50.0f, WHITE);
//
//     rlEnableDepthMask();
//     rlEnableBackfaceCulling();
// }

} // namespace renderer
