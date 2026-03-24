#include "skybox.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>

namespace renderer {

Skybox& Skybox::instance() {
    static Skybox g;
    return g;
}

bool Skybox::init(rf::RenderDevice& device) {
    if (ready_) return true;
    device_ = &device;

    // Load skybox shader
    shader_ = device.createShader();
    if (!shader_->loadFromFiles("shaders/skybox.vs", "shaders/skybox.fs")) {
        TraceLog(LOG_WARNING, "[Skybox] Shader not found, skybox disabled");
        ready_ = false;
        return false;
    }

    // Create unit cube mesh (position-only)
    cubeMesh_ = device.createMesh();
    {
        float s = 0.5f;
        // clang-format off
        float positions[] = {
            s, -s, -s,  s,  s, -s,  s,  s,  s,
            s, -s, -s,  s,  s,  s,  s, -s,  s,
           -s, -s,  s, -s,  s,  s, -s,  s, -s,
           -s, -s,  s, -s,  s, -s, -s, -s, -s,
           -s,  s, -s, -s,  s,  s,  s,  s,  s,
           -s,  s, -s,  s,  s,  s,  s,  s, -s,
           -s, -s,  s, -s, -s, -s,  s, -s, -s,
           -s, -s,  s,  s, -s, -s,  s, -s,  s,
           -s, -s,  s,  s, -s,  s,  s,  s,  s,
           -s, -s,  s,  s,  s,  s, -s,  s,  s,
            s, -s, -s, -s, -s, -s, -s,  s, -s,
            s, -s, -s, -s,  s, -s,  s,  s, -s,
        };
        // clang-format on
        cubeMesh_->uploadPositionOnly(positions, 36);
    }

    // Set cubemap sampler uniform
    shader_->bind();
    shader_->setInt("environmentMap", 0);
    shader_->unbind();

    ready_ = true;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;

    TraceLog(LOG_INFO, "[Skybox] Initialized");
    return true;
}

void Skybox::shutdown() {
    cubemap_.reset();
    cubeMesh_.reset();
    shader_.reset();
    device_ = nullptr;
    ready_ = false;
    loaded_kind_ = shared::maps::MapTemplate::SkyboxKind::None;
    TraceLog(LOG_INFO, "[Skybox] Shutdown");
}

void Skybox::set_kind(shared::maps::MapTemplate::SkyboxKind kind) {
    kind_ = kind;
}

void Skybox::draw(const rf::Camera& camera) {
    if (!ready_ || !shader_ || !shader_->isValid()) return;
    if (kind_ == shared::maps::MapTemplate::SkyboxKind::None) return;

    ensure_cubemap_loaded_();
    if (!cubemap_ || !cubemap_->isValid()) return;

    // Strip translation from view matrix so skybox stays centred on the camera
    rf::Mat4 view = rf::Mat4(rf::Mat3(camera.viewMatrix()));
    rf::Mat4 proj = camera.projectionMatrix();
    rf::Mat4 mvp  = proj * view;

    // Draw behind everything
    device_->setDepthFunc(rf::DepthFunc::LessEqual);
    device_->setDepthWrite(false);
    device_->setCullMode(rf::CullMode::None);  // Camera is inside the cube

    shader_->bind();
    shader_->setMat4("mvp", mvp);

    cubemap_->bind(0);
    cubeMesh_->draw();
    cubemap_->unbind(0);

    shader_->unbind();

    device_->setCullMode(rf::CullMode::None);
    device_->setDepthWrite(true);
    device_->setDepthFunc(rf::DepthFunc::Less);
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
    if (!ready_ || !device_) return;
    if (kind_ == loaded_kind_ && cubemap_ && cubemap_->isValid()) return;

    // Destroy previous cubemap
    cubemap_.reset();

    // Try loading from equirectangular panorama
    const char* pano = panorama_path_for_kind_(kind_);
    if (pano) {
        cubemap_ = device_->createTexture();
        if (cubemap_->loadCubemapFromPanorama(pano, 512)) {
            loaded_kind_ = kind_;
            TraceLog(LOG_INFO, "[Skybox] Cubemap loaded from panorama: %s", pano);
            return;
        }
        TraceLog(LOG_WARNING, "[Skybox] Panorama load failed: %s", pano);
    }

    // Fallback: try loading as a single panorama-style image via the cubemap path
    const char* cube = cubemap_path_for_kind_(kind_);
    if (cube) {
        cubemap_ = device_->createTexture();
        if (cubemap_->loadCubemapFromPanorama(cube, 512)) {
            loaded_kind_ = kind_;
            TraceLog(LOG_INFO, "[Skybox] Cubemap loaded from cubemap image as panorama: %s", cube);
            return;
        }
    }

    TraceLog(LOG_WARNING, "[Skybox] Failed to load cubemap for kind %d",
             static_cast<int>(kind_));
}

} // namespace renderer
