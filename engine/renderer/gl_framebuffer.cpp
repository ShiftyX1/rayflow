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
    , colorTextures_(std::move(other.colorTextures_))
    , depthRbo_(other.depthRbo_)
    , depthTex_(other.depthTex_)
    , width_(other.width_)
    , height_(other.height_)
    , hasDepth_(other.hasDepth_)
    , depthAsTexture_(other.depthAsTexture_)
    , depthOnly_(other.depthOnly_)
    , depthFormat_(other.depthFormat_)
    , attachmentDescs_(std::move(other.attachmentDescs_))
{
    other.fbo_ = 0;
    other.depthRbo_ = 0;
    other.depthTex_ = 0;
}

GLFramebuffer& GLFramebuffer::operator=(GLFramebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        fbo_ = other.fbo_;
        colorTextures_ = std::move(other.colorTextures_);
        depthRbo_ = other.depthRbo_;
        depthTex_ = other.depthTex_;
        width_ = other.width_;
        height_ = other.height_;
        hasDepth_ = other.hasDepth_;
        depthAsTexture_ = other.depthAsTexture_;
        depthOnly_ = other.depthOnly_;
        depthFormat_ = other.depthFormat_;
        attachmentDescs_ = std::move(other.attachmentDescs_);
        other.fbo_ = 0;
        other.depthRbo_ = 0;
        other.depthTex_ = 0;
    }
    return *this;
}

// ============================================================================
// Simple single-attachment create (backwards compatible)
// ============================================================================

bool GLFramebuffer::create(int width, int height, bool /*withDepth*/) {
    FBAttachment att;
    att.internalFormat = GL_RGBA8;
    att.filter = GL_LINEAR;
    return createMRT(width, height, {att}, false);
}

// ============================================================================
// MRT / HDR create
// ============================================================================

bool GLFramebuffer::createMRT(int width, int height,
                               const std::vector<FBAttachment>& attachments,
                               bool depthAsTexture)
{
    destroy();

    width_ = width;
    height_ = height;
    hasDepth_ = true;
    depthAsTexture_ = depthAsTexture;
    depthOnly_ = false;
    depthFormat_ = GL_DEPTH_COMPONENT24;
    attachmentDescs_ = attachments;

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Color attachments
    colorTextures_.resize(attachments.size());
    glGenTextures(static_cast<GLsizei>(attachments.size()), colorTextures_.data());

    std::vector<GLenum> drawBuffers;
    for (int i = 0; i < static_cast<int>(attachments.size()); ++i) {
        const auto& att = attachments[i];
        GLenum target = GL_COLOR_ATTACHMENT0 + i;
        drawBuffers.push_back(target);

        glBindTexture(GL_TEXTURE_2D, colorTextures_[i]);

        // Determine pixel type from internal format
        GLenum pixelType = GL_UNSIGNED_BYTE;
        GLenum pixelFormat = GL_RGBA;
        if (att.internalFormat == GL_RGBA16F || att.internalFormat == GL_RGBA32F) {
            pixelType = GL_FLOAT;
        } else if (att.internalFormat == GL_RG16F) {
            pixelType = GL_FLOAT;
            pixelFormat = GL_RG;
        } else if (att.internalFormat == GL_R16F || att.internalFormat == GL_R32F) {
            pixelType = GL_FLOAT;
            pixelFormat = GL_RED;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, att.internalFormat, width, height, 0,
                     pixelFormat, pixelType, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, att.filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, att.filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, att.wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, att.wrap);

        glFramebufferTexture2D(GL_FRAMEBUFFER, target, GL_TEXTURE_2D, colorTextures_[i], 0);
    }

    glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

    // Depth attachment
    if (depthAsTexture) {
        glGenTextures(1, &depthTex_);
        glBindTexture(GL_TEXTURE_2D, depthTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);
    } else {
        glGenRenderbuffers(1, &depthRbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "[GLFramebuffer] Incomplete FBO (status 0x%X)", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    TraceLog(LOG_INFO, "[GLFramebuffer] Created MRT %dx%d (%d attachments, depthTex=%s)",
             width, height, static_cast<int>(attachments.size()),
             depthAsTexture ? "yes" : "no");
    return true;
}

// ============================================================================
// Depth-only create (shadow maps)
// ============================================================================

bool GLFramebuffer::createDepthOnly(int width, int height, GLenum depthFormat) {
    destroy();

    width_ = width;
    height_ = height;
    hasDepth_ = true;
    depthAsTexture_ = true;
    depthOnly_ = true;
    depthFormat_ = depthFormat;
    attachmentDescs_.clear();

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Depth texture
    glGenTextures(1, &depthTex_);
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, depthFormat, width, height, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // Border color = 1.0 (no shadow outside the map)
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Enable hardware shadow comparison
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);

    // No color output
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "[GLFramebuffer] Incomplete depth-only FBO (status 0x%X)", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    TraceLog(LOG_INFO, "[GLFramebuffer] Created depth-only %dx%d", width, height);
    return true;
}

// ============================================================================
// Resize / Destroy
// ============================================================================

bool GLFramebuffer::resize(int width, int height) {
    if (depthOnly_) {
        return createDepthOnly(width, height, depthFormat_);
    }
    if (!attachmentDescs_.empty()) {
        return createMRT(width, height, attachmentDescs_, depthAsTexture_);
    }
    return create(width, height, hasDepth_);
}

void GLFramebuffer::destroy() {
    if (depthRbo_) {
        glDeleteRenderbuffers(1, &depthRbo_);
        depthRbo_ = 0;
    }
    if (depthTex_) {
        glDeleteTextures(1, &depthTex_);
        depthTex_ = 0;
    }
    if (!colorTextures_.empty()) {
        glDeleteTextures(static_cast<GLsizei>(colorTextures_.size()), colorTextures_.data());
        colorTextures_.clear();
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

// ============================================================================
// Texture binding helpers
// ============================================================================

void GLFramebuffer::bindColorTexture(int index, int unit) const {
    if (index >= 0 && index < static_cast<int>(colorTextures_.size())) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, colorTextures_[index]);
    }
}

void GLFramebuffer::bindDepthTexture(int unit) const {
    if (depthTex_) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, depthTex_);
    }
}

GLuint GLFramebuffer::colorTexture(int index) const {
    if (index >= 0 && index < static_cast<int>(colorTextures_.size())) {
        return colorTextures_[index];
    }
    return 0;
}

} // namespace rf
