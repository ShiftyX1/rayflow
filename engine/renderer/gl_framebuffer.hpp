#pragma once

// =============================================================================
// GLFramebuffer — OpenGL FBO wrapper (render-to-texture)
//
// Supports:
//   - Single or multiple color attachments (MRT)
//   - HDR internal formats (GL_RGBA16F)
//   - Depth as texture (for shadow mapping, post-processing reads)
//   - Depth-only mode (no color attachment — shadow maps)
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/renderer/gpu/gpu_framebuffer.hpp"

#include <glad/gl.h>
#include <vector>

namespace rf {

class GLTexture; // forward decl — we expose raw texture IDs, not owning ptrs

/// Describes a single color attachment (GL-specific).
struct FBAttachment {
    GLenum internalFormat = GL_RGBA8;       ///< e.g. GL_RGBA16F, GL_RGBA8
    GLenum filter         = GL_LINEAR;      ///< GL_LINEAR or GL_NEAREST
    GLenum wrap           = GL_CLAMP_TO_EDGE;
};

class RAYFLOW_CLIENT_API GLFramebuffer : public IFramebuffer {
public:
    GLFramebuffer() = default;
    ~GLFramebuffer() override;

    // Non-copyable, movable
    GLFramebuffer(const GLFramebuffer&) = delete;
    GLFramebuffer& operator=(const GLFramebuffer&) = delete;
    GLFramebuffer(GLFramebuffer&& other) noexcept;
    GLFramebuffer& operator=(GLFramebuffer&& other) noexcept;

    // ---- IFramebuffer interface ----

    bool create(int width, int height, bool withDepth = true) override;

    /// IFramebuffer MRT create with abstract attachment descriptors.
    bool createMRT(int width, int height,
                   const std::vector<AttachmentDesc>& attachments,
                   bool depthAsTexture = false) override;

    /// IFramebuffer depth-only create with abstract depth format.
    bool createDepthOnly(int width, int height,
                         DepthFormat depthFormat = DepthFormat::Depth24) override;

    bool resize(int width, int height) override;
    void destroy() override;
    void bind() const override;
    void unbind() const override;
    void begin() const override;
    void end() const override;
    void bindColorTexture(int index, int unit) const override;
    void bindDepthTexture(int unit) const override;

    bool isValid() const override { return fbo_ != 0; }
    int width() const override { return width_; }
    int height() const override { return height_; }
    int colorAttachmentCount() const override { return static_cast<int>(colorTextures_.size()); }
    std::uintptr_t nativeColorHandle(int index = 0) const override;
    std::uintptr_t nativeDepthHandle() const override { return static_cast<std::uintptr_t>(depthTex_); }

    // ---- GL-specific API (for backward compatibility / internal use) ----

    /// Create MRT with GL-specific attachment descriptors.
    bool createMRT_GL(int width, int height,
                      const std::vector<FBAttachment>& attachments,
                      bool depthAsTexture = false);

    /// Create depth-only with raw GL format.
    bool createDepthOnly_GL(int width, int height,
                            GLenum depthFormat = GL_DEPTH_COMPONENT24);

    GLuint id() const { return fbo_; }
    GLuint colorTexture(int index = 0) const;
    GLuint depthTexture() const { return depthTex_; }

private:
    GLuint fbo_{0};

    // Color attachments (MRT)
    std::vector<GLuint> colorTextures_;

    // Depth (either renderbuffer OR texture, never both)
    GLuint depthRbo_{0};
    GLuint depthTex_{0};

    int width_{0};
    int height_{0};

    // Remembered for resize
    bool hasDepth_{false};
    bool depthAsTexture_{false};
    bool depthOnly_{false};
    GLenum depthFormat_{GL_DEPTH_COMPONENT24};
    std::vector<FBAttachment> attachmentDescs_;
};

} // namespace rf
