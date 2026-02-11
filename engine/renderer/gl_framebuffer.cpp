#include "gl_framebuffer.hpp"

#include "engine/core/logging.hpp"

namespace rf {

// ============================================================================
// Move semantics
// ============================================================================

GLFramebuffer::~GLFramebuffer() {
    destroy();
}

GLFramebuffer::GLFramebuffer(GLFramebuffer&& other) noexcept
    : fbo_(other.fbo_)
    , colorTex_(other.colorTex_)
    , depthRbo_(other.depthRbo_)
    , width_(other.width_)
    , height_(other.height_)
    , hasDepth_(other.hasDepth_)
{
    other.fbo_ = 0;
    other.colorTex_ = 0;
    other.depthRbo_ = 0;
}

GLFramebuffer& GLFramebuffer::operator=(GLFramebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        fbo_ = other.fbo_;
        colorTex_ = other.colorTex_;
        depthRbo_ = other.depthRbo_;
        width_ = other.width_;
        height_ = other.height_;
        hasDepth_ = other.hasDepth_;
        other.fbo_ = 0;
        other.colorTex_ = 0;
        other.depthRbo_ = 0;
    }
    return *this;
}

// ============================================================================
// Create / Resize / Destroy
// ============================================================================

bool GLFramebuffer::create(int width, int height, bool withDepth) {
    destroy();

    width_ = width;
    height_ = height;
    hasDepth_ = withDepth;

    // Create FBO
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Color attachment (RGBA texture)
    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

    // Depth attachment (renderbuffer)
    if (withDepth) {
        glGenRenderbuffers(1, &depthRbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);
    }

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "[GLFramebuffer] Incomplete FBO (status 0x%X)", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    TraceLog(LOG_INFO, "[GLFramebuffer] Created %dx%d (depth=%s)", width, height, withDepth ? "yes" : "no");
    return true;
}

bool GLFramebuffer::resize(int width, int height) {
    bool depth = hasDepth_;
    destroy();
    return create(width, height, depth);
}

void GLFramebuffer::destroy() {
    if (depthRbo_) {
        glDeleteRenderbuffers(1, &depthRbo_);
        depthRbo_ = 0;
    }
    if (colorTex_) {
        glDeleteTextures(1, &colorTex_);
        colorTex_ = 0;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

// ============================================================================
// Bind / Unbind
// ============================================================================

void GLFramebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void GLFramebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer::begin() const {
    bind();
    glViewport(0, 0, width_, height_);
}

void GLFramebuffer::end() const {
    unbind();
}

} // namespace rf
