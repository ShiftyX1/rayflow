#include "render_pipeline.hpp"

#include "engine/core/logging.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace rf {

// =============================================================================
// Frustum
// =============================================================================

void Frustum::extractFromVP(const Mat4& vp) {
    // Gribb/Hartmann method: extract 6 planes from VP matrix rows
    for (int i = 0; i < 4; ++i) {
        // Left
        planes[0][i] = vp[i][3] + vp[i][0];
        // Right
        planes[1][i] = vp[i][3] - vp[i][0];
        // Bottom
        planes[2][i] = vp[i][3] + vp[i][1];
        // Top
        planes[3][i] = vp[i][3] - vp[i][1];
        // Near
        planes[4][i] = vp[i][3] + vp[i][2];
        // Far
        planes[5][i] = vp[i][3] - vp[i][2];
    }

    // Normalize all planes
    for (auto& plane : planes) {
        float len = glm::length(Vec3(plane));
        if (len > 0.0f) plane /= len;
    }
}

bool Frustum::testAABB(const Vec3& minPt, const Vec3& maxPt) const {
    for (const auto& plane : planes) {
        Vec3 normal(plane);

        // Find the positive vertex (furthest along normal)
        Vec3 pVertex;
        pVertex.x = (normal.x >= 0.0f) ? maxPt.x : minPt.x;
        pVertex.y = (normal.y >= 0.0f) ? maxPt.y : minPt.y;
        pVertex.z = (normal.z >= 0.0f) ? maxPt.z : minPt.z;

        if (glm::dot(normal, pVertex) + plane.w < 0.0f) {
            return false; // Entirely outside this plane
        }
    }
    return true;
}

// =============================================================================
// Init / Shutdown
// =============================================================================

bool RenderPipeline::init(RenderDevice& device, int width, int height) {
    device_ = &device;
    width_ = width;
    height_ = height;

    // Create HDR scene FBO (RGBA16F + depth texture)
    sceneFBO_ = device.createFramebuffer();
    AttachmentDesc hdrAtt;
    hdrAtt.format = TextureFormat::RGBA16F;
    hdrAtt.filter = TextureFilter::Linear;
    if (!sceneFBO_->createMRT(width, height, {hdrAtt}, true)) {
        TraceLog(LOG_ERROR, "[RenderPipeline] Failed to create HDR scene FBO");
        return false;
    }

    // Create shadow map FBO (depth-only)
    shadowFBO_ = device.createFramebuffer();
    if (!shadowFBO_->createDepthOnly(shadowMapSize_, shadowMapSize_)) {
        TraceLog(LOG_ERROR, "[RenderPipeline] Failed to create shadow FBO");
        return false;
    }

    // Load shadow shader
    shadowShader_ = device.createShader();
    if (!shadowShader_->loadFromFiles("shaders/shadow.vs", "shaders/shadow.fs")) {
        TraceLog(LOG_WARNING, "[RenderPipeline] Shadow shader failed to load — shadows disabled");
    }

    // Load tonemap shader
    tonemapShader_ = device.createShader();
    if (!tonemapShader_->loadFromFiles("shaders/tonemap.vs", "shaders/tonemap.fs")) {
        TraceLog(LOG_ERROR, "[RenderPipeline] Tonemap shader failed to load");
        return false;
    }

    // Create fullscreen triangle (one large triangle covering clip space)
    fullscreenTriangle_ = device.createMesh();
    {
        float positions[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f,
        };
        fullscreenTriangle_->uploadPositionOnly(positions, 3);
    }

    initialized_ = true;
    TraceLog(LOG_INFO, "[RenderPipeline] Initialized (%dx%d, shadow=%d)", width, height, shadowMapSize_);
    return true;
}

void RenderPipeline::shutdown() {
    sceneFBO_.reset();
    shadowFBO_.reset();
    shadowShader_.reset();
    tonemapShader_.reset();
    fullscreenTriangle_.reset();
    device_ = nullptr;
    initialized_ = false;
}

void RenderPipeline::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    sceneFBO_->resize(width, height);
    TraceLog(LOG_INFO, "[RenderPipeline] Resized to %dx%d", width, height);
}

// =============================================================================
// Shadow Pass
// =============================================================================

void RenderPipeline::beginShadowPass(const Camera& camera, const Vec3& sunDir) {
    if (!shadowShader_ || !shadowShader_->isValid()) return;


    Vec3 lightDir = glm::normalize(sunDir);
    Vec3 lightUp  = (std::abs(lightDir.y) > 0.99f) ? Vec3(0, 0, 1) : Vec3(0, 1, 0);

    Mat4 lightView = glm::lookAt(Vec3(0.0f), -lightDir, lightUp);

    // Shadow frustum follows the camera (offset slightly forward)
    Vec3 center = camera.position() + camera.forward() * (shadowDistance_ * 0.4f);

    // Project center into light-space and snap to texel grid
    float halfDist  = shadowDistance_;
    float texelSize = (halfDist * 2.0f) / static_cast<float>(shadowMapSize_);
    Vec4 centerLS   = lightView * Vec4(center, 1.0f);
    centerLS.x = std::floor(centerLS.x / texelSize) * texelSize;
    centerLS.y = std::floor(centerLS.y / texelSize) * texelSize;

    // Build ortho projection centered on the snapped light-space position
    float nearPlane = -shadowDistance_ * 2.0f;
    float farPlane  =  shadowDistance_ * 2.0f;
    Mat4 lightProj = glm::ortho(
        centerLS.x - halfDist, centerLS.x + halfDist,
        centerLS.y - halfDist, centerLS.y + halfDist,
        nearPlane, farPlane);

    lightSpaceMatrix_ = lightProj * lightView;

    // Bind shadow FBO
    device_->getViewport(savedViewport_);
    shadowFBO_->begin();
    device_->clear(false, true);

    // Depth testing for shadow
    device_->setDepthTest(true);
    device_->setDepthFunc(DepthFunc::Less);

    // Slight polygon offset to reduce shadow acne
    device_->setPolygonOffset(true, 2.0f, 4.0f);

    shadowShader_->bind();
    shadowShader_->setMat4("lightSpaceMatrix", lightSpaceMatrix_);
}

void RenderPipeline::endShadowPass() {
    if (!shadowShader_ || !shadowShader_->isValid()) return;

    shadowShader_->unbind();
    device_->setPolygonOffset(false);
    shadowFBO_->end();

    // Restore viewport
    device_->setViewport(savedViewport_[0], savedViewport_[1], savedViewport_[2], savedViewport_[3]);
}

void RenderPipeline::bindShadowMap(int unit) const {
    shadowFBO_->bindDepthTexture(unit);
}

// =============================================================================
// HDR Scene Pass
// =============================================================================

void RenderPipeline::beginScene() {
    device_->getViewport(savedViewport_);
    sceneFBO_->begin();
    device_->setClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    device_->clear(true, true);
}

void RenderPipeline::endScene() {
    sceneFBO_->end();
    device_->setViewport(savedViewport_[0], savedViewport_[1], savedViewport_[2], savedViewport_[3]);
}

// =============================================================================
// Post-Processing (Tone Mapping)
// =============================================================================

void RenderPipeline::postProcess(float exposure) {
    device_->setDepthTest(false);

    // Bind tonemap shader
    tonemapShader_->bind();
    tonemapShader_->setFloat("exposure", exposure);
    tonemapShader_->setInt("hdrBuffer", 0);

    // Bind HDR scene color texture
    sceneFBO_->bindColorTexture(0, 0);

    // Draw fullscreen triangle
    fullscreenTriangle_->draw();

    tonemapShader_->unbind();
}

// =============================================================================
// Frustum Culling
// =============================================================================

void RenderPipeline::updateFrustum(const Camera& camera) {
    Mat4 vp = camera.viewProjectionMatrix();
    frustum_.extractFromVP(vp);
}

bool RenderPipeline::isChunkVisible(int chunkX, int chunkZ, int chunkHeight) const {
    // Chunk AABB in world space
    Vec3 minPt(static_cast<float>(chunkX * 16), 0.0f, static_cast<float>(chunkZ * 16));
    Vec3 maxPt(static_cast<float>(chunkX * 16 + 16), static_cast<float>(chunkHeight),
               static_cast<float>(chunkZ * 16 + 16));
    return frustum_.testAABB(minPt, maxPt);
}

} // namespace rf
