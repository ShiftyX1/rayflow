#include "skybox.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>

namespace renderer {

Skybox& Skybox::instance() {
    static Skybox g;
    return g;
}

bool Skybox::init() {
    if (ready_) return true;

    // Load skybox shader
    if (!shader_.loadFromFiles("shaders/skybox.vs", "shaders/skybox.fs")) {
        TraceLog(LOG_WARNING, "[Skybox] Shader not found, skybox disabled");
        ready_ = false;
        return false;
    }

    // Create unit cube mesh (position-only)
    cubeMesh_ = rf::GLMesh::createCube(1.0f);

    // Set cubemap sampler uniform
    shader_.bind();
    shader_.setInt("environmentMap", 0);
    rf::GLShader::unbind();

    ready_ = true;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;

    TraceLog(LOG_INFO, "[Skybox] Initialized");
    return true;
}

void Skybox::shutdown() {
    cubemap_.destroy();
    cubeMesh_.destroy();
    shader_.destroy();
    ready_ = false;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
    TraceLog(LOG_INFO, "[Skybox] Shutdown");
}

void Skybox::set_kind(shared::maps::MapTemplate::SkyboxKind kind) {
    kind_ = kind;
}

void Skybox::draw(const rf::Camera& camera) {
    if (!ready_ || !shader_.isValid()) return;
    if (kind_ == shared::maps::MapTemplate::SkyboxKind::None) return;

    ensure_cubemap_loaded_();
    if (!cubemap_.isValid()) return;

    // Strip translation from view matrix so skybox stays centred on the camera
    rf::Mat4 view = rf::Mat4(rf::Mat3(camera.viewMatrix()));
    rf::Mat4 proj = camera.projectionMatrix();
    rf::Mat4 mvp  = proj * view;

    // Draw behind everything
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    shader_.bind();
    shader_.setMat4("mvp", mvp);

    cubemap_.bind(0);
    cubeMesh_.draw();
    rf::GLTexture::unbind(0);

    rf::GLShader::unbind();

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

const char* Skybox::panorama_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind) {
    const std::uint8_t id = static_cast<std::uint8_t>(kind);
    if (id == 0) return nullptr;

    char buf[128];
    std::snprintf(buf, sizeof(buf), "textures/skybox/panorama/Panorama_Sky_%02u-512x512.png",
                  static_cast<unsigned>(id));
    pano_path_ = buf;
    return pano_path_.c_str();
}

const char* Skybox::cubemap_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind) {
    const std::uint8_t id = static_cast<std::uint8_t>(kind);
    if (id == 0) return nullptr;

    char buf[128];
    std::snprintf(buf, sizeof(buf), "textures/skybox/cubemap/Cubemap_Sky_%02u-512x512.png",
                  static_cast<unsigned>(id));
    cube_path_ = buf;
    return cube_path_.c_str();
}

void Skybox::ensure_cubemap_loaded_() {
    if (!ready_) return;
    if (kind_ == loaded_kind_ && cubemap_.isValid()) return;

    // Destroy previous cubemap
    cubemap_.destroy();

    // Try loading from equirectangular panorama
    const char* pano = panorama_path_for_kind_(kind_);
    if (pano) {
        if (cubemap_.loadCubemapFromPanorama(pano, 512)) {
            loaded_kind_ = kind_;
            TraceLog(LOG_INFO, "[Skybox] Cubemap loaded from panorama: %s", pano);
            return;
        }
    }

    TraceLog(LOG_WARNING, "[Skybox] Failed to load cubemap for kind %d",
             static_cast<int>(kind_));
}

} // namespace renderer
