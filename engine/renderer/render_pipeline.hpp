#pragma once

// =============================================================================
// RenderPipeline — HDR scene rendering + post-processing chain
//
// Manages:
//   - HDR scene FBO (RGBA16F color + depth texture)
//   - Shadow map pass
//   - Tone mapping post-process
//   - Frustum culling
// =============================================================================

#include "engine/renderer/gl_framebuffer.hpp"
#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/gl_mesh.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/core/math_types.hpp"

#include <glad/gl.h>
#include <array>

namespace rf {

/// Frustum planes for culling (extracted from VP matrix).
struct Frustum {
    /// Each plane: (a, b, c, d) where ax + by + cz + d = 0.
    std::array<Vec4, 6> planes;

    /// Extract frustum planes from a view-projection matrix.
    void extractFromVP(const Mat4& vp);

    /// Test an AABB against the frustum.
    /// @return true if the box is (at least partially) inside the frustum.
    bool testAABB(const Vec3& minPt, const Vec3& maxPt) const;
};

class RenderPipeline {
public:
    RenderPipeline() = default;
    ~RenderPipeline() = default;

    // Non-copyable
    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;

    /// Initialize the pipeline (creates FBOs, loads shaders).
    /// @param width, height  Initial framebuffer dimensions.
    bool init(int width, int height);

    /// Shutdown and release all GPU resources.
    void shutdown();

    /// Resize all FBOs (call on window resize).
    void resize(int width, int height);

    // ---- Shadow pass ----

    /// Begin shadow depth pass. Binds shadow FBO and sets up light-space VP.
    /// @param camera     Player camera (used to position shadow frustum).
    /// @param sunDir     Normalized sun direction.
    void beginShadowPass(const Camera& camera, const Vec3& sunDir);

    /// End shadow pass. Unbinds shadow FBO.
    void endShadowPass();

    /// Get the shadow depth shader (for external chunk rendering).
    GLShader& shadowShader() { return shadowShader_; }

    /// Light-space VP matrix (valid after beginShadowPass).
    const Mat4& lightSpaceMatrix() const { return lightSpaceMatrix_; }

    /// Bind shadow map depth texture to a texture unit.
    void bindShadowMap(int unit) const;

    // ----- HDR scene pass -----

    /// Begin HDR scene rendering. Binds scene FBO, clears.
    void beginScene();

    /// End scene rendering. Unbinds scene FBO.
    void endScene();

    // ----- Post-processing -----

    /// Run the tone mapping pass (HDR scene FBO → default framebuffer).
    /// @param exposure  Exposure multiplier.
    void postProcess(float exposure = 1.0f);

    // ----- Frustum culling -----

    /// Extract frustum from current camera VP.
    void updateFrustum(const Camera& camera);

    /// Test chunk AABB against the current frustum.
    bool isChunkVisible(int chunkX, int chunkZ, int chunkHeight = 256) const;

    // ----- Accessors -----

    int width() const { return width_; }
    int height() const { return height_; }
    bool isInitialized() const { return initialized_; }

    /// Shadow map resolution.
    int shadowMapSize() const { return shadowMapSize_; }
    void setShadowMapSize(int size) { shadowMapSize_ = size; }

    /// Shadow distance (world units).
    float shadowDistance() const { return shadowDistance_; }
    void setShadowDistance(float dist) { shadowDistance_ = dist; }

private:
    bool initialized_{false};
    int width_{0};
    int height_{0};

    // ---- HDR Scene FBO ----
    GLFramebuffer sceneFBO_;

    // ---- Shadow map ----
    GLFramebuffer shadowFBO_;
    GLShader shadowShader_;
    Mat4 lightSpaceMatrix_{1.0f};
    int shadowMapSize_{2048};
    float shadowDistance_{128.0f};

    // ---- Tone mapping ----
    GLShader tonemapShader_;
    GLMesh fullscreenTriangle_;

    // ---- Frustum ----
    Frustum frustum_;

    // ---- Saved viewport (restored after post-process) ----
    int savedViewport_[4]{};
};

} // namespace rf
