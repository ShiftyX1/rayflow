#pragma once

// =============================================================================
// GLFramebuffer — OpenGL FBO wrapper (render-to-texture)
//
// Phase 2: Replaces raylib RenderTexture2D / LoadRenderTexture /
//          BeginTextureMode / EndTextureMode.
// =============================================================================

#include <glad/gl.h>

namespace rf {

class GLTexture; // forward decl — we expose raw texture IDs, not owning ptrs

class GLFramebuffer {
public:
    GLFramebuffer() = default;
    ~GLFramebuffer();

    // Non-copyable, movable
    GLFramebuffer(const GLFramebuffer&) = delete;
    GLFramebuffer& operator=(const GLFramebuffer&) = delete;
    GLFramebuffer(GLFramebuffer&& other) noexcept;
    GLFramebuffer& operator=(GLFramebuffer&& other) noexcept;

    /// Create an FBO with a color attachment and optional depth attachment.
    /// @param width, height   Dimensions in pixels.
    /// @param withDepth       Create a depth renderbuffer.
    /// @return true on success.
    bool create(int width, int height, bool withDepth = true);

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

    // ----- Accessors -----

    bool isValid() const { return fbo_ != 0; }
    GLuint id() const { return fbo_; }
    GLuint colorTexture() const { return colorTex_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    GLuint fbo_{0};
    GLuint colorTex_{0};
    GLuint depthRbo_{0};
    int width_{0};
    int height_{0};
    bool hasDepth_{false};
};

} // namespace rf
