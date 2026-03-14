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

#include <glad/gl.h>
#include <vector>

namespace rf {

class GLTexture; // forward decl — we expose raw texture IDs, not owning ptrs

/// Describes a single color attachment.
struct FBAttachment {
    GLenum internalFormat = GL_RGBA8;       ///< e.g. GL_RGBA16F, GL_RGBA8
    GLenum filter         = GL_LINEAR;      ///< GL_LINEAR or GL_NEAREST
    GLenum wrap           = GL_CLAMP_TO_EDGE;
};

class RAYFLOW_CLIENT_API GLFramebuffer {
public:
    GLFramebuffer() = default;
    ~GLFramebuffer();

    // Non-copyable, movable
    GLFramebuffer(const GLFramebuffer&) = delete;
    GLFramebuffer& operator=(const GLFramebuffer&) = delete;
    GLFramebuffer(GLFramebuffer&& other) noexcept;
    GLFramebuffer& operator=(GLFramebuffer&& other) noexcept;

    // ---- Simple single-attachment create (backwards compatible) ----

    /// Create an FBO with a color attachment and optional depth attachment.
    /// @param width, height   Dimensions in pixels.
    /// @param withDepth       Create a depth renderbuffer.
    /// @return true on success.
    bool create(int width, int height, bool withDepth = true);

    // ---- MRT / HDR create ----

    /// Create an FBO with multiple color attachments.
    /// @param attachments  Vector of attachment descriptors (format, filter).
    /// @param depthAsTexture  If true, depth is stored as a texture (readable in shaders).
    ///                        If false, depth is a renderbuffer (faster, write-only).
    bool createMRT(int width, int height,
                   const std::vector<FBAttachment>& attachments,
                   bool depthAsTexture = false);

    // ---- Depth-only create (shadow maps) ----

    /// Create a depth-only FBO (no color attachment).
    /// @param depthFormat  GL_DEPTH_COMPONENT16 / 24 / 32F.
    bool createDepthOnly(int width, int height,
                         GLenum depthFormat = GL_DEPTH_COMPONENT24);

    /// Resize the FBO (destroys and recreates attachments).
    bool resize(int width, int height);

    /// Destroy the FBO and all attachments.
    void destroy();

    /// Bind this FBO (all subsequent draws go here).
    void bind() const;

    /// Unbind — revert to default framebuffer.
    static void unbind();

    /// Begin rendering (bind + set viewport). Convenience wrapper.
    void begin() const;

    /// End rendering (unbind). Caller should restore viewport.
    void end() const;

    // ---- Texture binding helpers ----

    /// Bind color attachment `index` to texture unit `unit`.
    void bindColorTexture(int index, int unit) const;

    /// Bind the depth texture to a texture unit (only if created with depthAsTexture).
    void bindDepthTexture(int unit) const;

    // ----- Accessors -----

    bool isValid() const { return fbo_ != 0; }
    GLuint id() const { return fbo_; }
    GLuint colorTexture(int index = 0) const;
    GLuint depthTexture() const { return depthTex_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int colorAttachmentCount() const { return static_cast<int>(colorTextures_.size()); }

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
