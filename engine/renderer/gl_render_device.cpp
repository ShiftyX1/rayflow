#include "gl_render_device.hpp"

#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/gl_texture.hpp"
#include "engine/renderer/gl_mesh.hpp"
#include "engine/renderer/gl_framebuffer.hpp"
#include "engine/core/logging.hpp"

#include <glad/gl.h>

namespace rf {

// ============================================================================
// Resource creation
// ============================================================================

std::unique_ptr<IShader> GLRenderDevice::createShader() {
    return std::make_unique<GLShader>();
}

std::unique_ptr<ITexture> GLRenderDevice::createTexture() {
    return std::make_unique<GLTexture>();
}

std::unique_ptr<IMesh> GLRenderDevice::createMesh() {
    return std::make_unique<GLMesh>();
}

std::unique_ptr<IFramebuffer> GLRenderDevice::createFramebuffer() {
    return std::make_unique<GLFramebuffer>();
}

// ============================================================================
// Global GPU state
// ============================================================================

void GLRenderDevice::setViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void GLRenderDevice::getViewport(int outViewport[4]) const {
    glGetIntegerv(GL_VIEWPORT, outViewport);
}

void GLRenderDevice::clear(bool color, bool depth) {
    GLbitfield mask = 0;
    if (color) mask |= GL_COLOR_BUFFER_BIT;
    if (depth) mask |= GL_DEPTH_BUFFER_BIT;
    if (mask) glClear(mask);
}

void GLRenderDevice::setClearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void GLRenderDevice::setDepthTest(bool enabled) {
    if (enabled) glEnable(GL_DEPTH_TEST);
    else         glDisable(GL_DEPTH_TEST);
}

void GLRenderDevice::setDepthFunc(DepthFunc func) {
    GLenum glFunc = GL_LESS;
    switch (func) {
        case DepthFunc::Less:         glFunc = GL_LESS;     break;
        case DepthFunc::LessEqual:    glFunc = GL_LEQUAL;   break;
        case DepthFunc::Greater:      glFunc = GL_GREATER;  break;
        case DepthFunc::GreaterEqual: glFunc = GL_GEQUAL;   break;
        case DepthFunc::Equal:        glFunc = GL_EQUAL;    break;
        case DepthFunc::Always:       glFunc = GL_ALWAYS;   break;
        case DepthFunc::Never:        glFunc = GL_NEVER;    break;
    }
    glDepthFunc(glFunc);
}

void GLRenderDevice::setDepthWrite(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void GLRenderDevice::setBlendMode(BlendMode mode) {
    switch (mode) {
        case BlendMode::None:
            glDisable(GL_BLEND);
            break;
        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case BlendMode::Premultiplied:
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            break;
    }
}

void GLRenderDevice::setCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None:
            glDisable(GL_CULL_FACE);
            break;
        case CullMode::Back:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case CullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
    }
}

void GLRenderDevice::setPolygonOffset(bool enabled, float factor, float units) {
    if (enabled) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(factor, units);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void GLRenderDevice::present() {
    // OpenGL swap is handled by GLFW — this is a no-op here.
}

// ============================================================================
// Initialization
// ============================================================================

bool GLRenderDevice::init(void* /*nativeWindowHandle*/, int width, int height) {
    // GLAD is already loaded by Window::init() for OpenGL.
    // Just verify we have a valid GL context by checking a known function pointer.
    if (!glGetString) {
        TraceLog(LOG_ERROR, "[GLRenderDevice] No valid GL context (GLAD not loaded)");
        return false;
    }

    TraceLog(LOG_INFO, "[GLRenderDevice] Initialized (%dx%d)", width, height);
    TraceLog(LOG_INFO, "[GLRenderDevice] GL Vendor: %s", glGetString(GL_VENDOR));
    TraceLog(LOG_INFO, "[GLRenderDevice] GL Renderer: %s", glGetString(GL_RENDERER));
    TraceLog(LOG_INFO, "[GLRenderDevice] GL Version: %s", glGetString(GL_VERSION));

    initialized_ = true;
    return true;
}

void GLRenderDevice::shutdown() {
    initialized_ = false;
    TraceLog(LOG_INFO, "[GLRenderDevice] Shutdown");
}

void GLRenderDevice::onResize(int width, int height) {
    glViewport(0, 0, width, height);
}

} // namespace rf
