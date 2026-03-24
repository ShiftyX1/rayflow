#include "batch_2d.hpp"

#include "engine/renderer/gpu/gpu_texture.hpp"
#include "engine/renderer/gpu/render_device.hpp"
#include "engine/client/core/window.hpp"
#include "engine/client/core/resources.hpp"
#include "gl_font.hpp"
#include "gl_shader.hpp"
#include "engine/core/logging.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#if RAYFLOW_HAS_DX11
#include "engine/renderer/dx11/dx11_render_device.hpp"
#include "engine/renderer/dx11/dx11_shader.hpp"
#include "engine/renderer/dx11/dx11_texture.hpp"
#include <stb_truetype.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#endif

#include <cmath>
#include <cstring>
#include <fstream>

namespace rf {

// ============================================================================
// Shader sources
// ============================================================================

static const char* kBatch2DVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

out vec2 vUV;
out vec4 vColor;

uniform mat4 uProjection;

void main() {
    vUV    = aUV;
    vColor = aColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kBatch2DFragSrc = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;

out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vUV) * vColor;
}
)";

// ============================================================================
// Singleton
// ============================================================================

Batch2D& Batch2D::instance() {
    static Batch2D s;
    return s;
}

Batch2D::~Batch2D() {
    if (!available_) return;

    if (backend_ == Backend::OpenGL) {
        if (glVao_) glDeleteVertexArrays(1, &glVao_);
        if (glVbo_) glDeleteBuffers(1, &glVbo_);
        if (glWhiteTex_) glDeleteTextures(1, &glWhiteTex_);
        if (glShader_) { glShader_->destroy(); delete glShader_; }
    }
#if RAYFLOW_HAS_DX11
    else if (backend_ == Backend::DirectX11) {
        destroyDX11();
    }
#endif
}

// ============================================================================
// Lazy init
// ============================================================================

void Batch2D::ensureInitialised() {
    if (initialised_) return;

    backend_ = Window::instance().backend();

#if RAYFLOW_HAS_DX11
    if (backend_ == Backend::DirectX11) {
        ensureInitialisedDX11();
        return;
    }
#endif

    if (backend_ != Backend::OpenGL) {
        initialised_ = true;
        available_ = false;
        TraceLog(LOG_INFO, "[Batch2D] Skipped (unsupported backend)");
        return;
    }

    // --- OpenGL path ---

    // Compile shader
    glShader_ = new GLShader();
    if (!glShader_->loadFromSource(kBatch2DVertSrc, kBatch2DFragSrc)) {
        TraceLog(LOG_ERROR, "[Batch2D] Failed to compile batch shader");
        delete glShader_;
        glShader_ = nullptr;
        return;
    }

    // Create VAO/VBO
    glGenVertexArrays(1, &glVao_);
    glGenBuffers(1, &glVbo_);

    glBindVertexArray(glVao_);
    glBindBuffer(GL_ARRAY_BUFFER, glVbo_);

    // Pre-allocate VBO
    glBufferData(GL_ARRAY_BUFFER,
                 kMaxVertices * sizeof(Vertex),
                 nullptr, GL_DYNAMIC_DRAW);

    constexpr int kStride = sizeof(Vertex);

    // pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<void*>(offsetof(Vertex, x)));

    // uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<void*>(offsetof(Vertex, u)));

    // color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, kStride,
                          reinterpret_cast<void*>(offsetof(Vertex, r)));

    glBindVertexArray(0);

    // Create 1x1 white texture
    std::uint8_t white[] = {255, 255, 255, 255};
    glGenTextures(1, &glWhiteTex_);
    glBindTexture(GL_TEXTURE_2D, glWhiteTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    whiteTexture_ = static_cast<std::uintptr_t>(glWhiteTex_);

    vertices_.reserve(4096);

    initialised_ = true;
    available_ = true;
    TraceLog(LOG_INFO, "[Batch2D] Initialised OpenGL (max %d vertices)", kMaxVertices);
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void Batch2D::begin(int screenWidth, int screenHeight) {
    ensureInitialised();
    if (!available_) return;

    projection_ = glm::ortho(0.0f, static_cast<float>(screenWidth),
                              static_cast<float>(screenHeight), 0.0f,
                              -1.0f, 1.0f);

    currentTexture_ = whiteTexture_;
    vertices_.clear();
    inFrame_ = true;

    if (backend_ == Backend::OpenGL) {
        // Also set projection for GLFont text rendering
        GLFont::setProjection(static_cast<float>(screenWidth),
                              static_cast<float>(screenHeight));

        // Setup GL state for 2D
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
#if RAYFLOW_HAS_DX11
    else if (backend_ == Backend::DirectX11) {
        dxDevice_->setDepthTest(false);
        dxDevice_->setCullMode(CullMode::None);
        dxDevice_->setBlendMode(BlendMode::Alpha);
        dxDevice_->flushState();
    }
#endif
}

void Batch2D::end() {
    if (!inFrame_) return;
    if (!available_) { inFrame_ = false; return; }
    flush();
    inFrame_ = false;

    if (backend_ == Backend::OpenGL) {
        // Restore common state
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
#if RAYFLOW_HAS_DX11
    else if (backend_ == Backend::DirectX11) {
        dxDevice_->setDepthTest(true);
        dxDevice_->setCullMode(CullMode::Back);
        dxDevice_->setBlendMode(BlendMode::None);
        dxDevice_->flushState();
    }
#endif
}

// ============================================================================
// Flush
// ============================================================================

void Batch2D::flush() {
    if (!available_ || vertices_.empty()) return;

#if RAYFLOW_HAS_DX11
    if (backend_ == Backend::DirectX11) {
        flushDX11();
        return;
    }
#endif

    // --- OpenGL path ---
    glShader_->bind();
    glShader_->setMat4("uProjection", projection_);
    glShader_->setInt("uTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(currentTexture_));

    glBindVertexArray(glVao_);
    glBindBuffer(GL_ARRAY_BUFFER, glVbo_);

    int count = static_cast<int>(vertices_.size());
    if (count > kMaxVertices) count = kMaxVertices;

    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    count * sizeof(Vertex),
                    vertices_.data());

    glDrawArrays(GL_TRIANGLES, 0, count);

    glBindVertexArray(0);
    glShader_->unbind();

    vertices_.clear();
}

void Batch2D::setTexture(std::uintptr_t texHandle) {
    if (!available_) return;
    if (texHandle == 0) texHandle = whiteTexture_;
    if (texHandle != currentTexture_) {
        flush();
        currentTexture_ = texHandle;
    }
}

// ============================================================================
// Primitive helpers
// ============================================================================

void Batch2D::pushQuad(float x0, float y0, float u0, float v0,
                       float x1, float y1, float u1, float v1,
                       float x2, float y2, float u2, float v2,
                       float x3, float y3, float u3, float v3,
                       float r, float g, float b, float a)
{
    if (!available_) return;
    if (static_cast<int>(vertices_.size()) + 6 > kMaxVertices) {
        flush();
    }

    // Two triangles: (0,1,2) and (0,2,3)
    vertices_.push_back({x0, y0, u0, v0, r, g, b, a});
    vertices_.push_back({x1, y1, u1, v1, r, g, b, a});
    vertices_.push_back({x2, y2, u2, v2, r, g, b, a});

    vertices_.push_back({x0, y0, u0, v0, r, g, b, a});
    vertices_.push_back({x2, y2, u2, v2, r, g, b, a});
    vertices_.push_back({x3, y3, u3, v3, r, g, b, a});
}

void Batch2D::pushTriangle(float x0, float y0, float u0, float v0,
                           float x1, float y1, float u1, float v1,
                           float x2, float y2, float u2, float v2,
                           float r, float g, float b, float a)
{
    if (!available_) return;
    if (static_cast<int>(vertices_.size()) + 3 > kMaxVertices) {
        flush();
    }

    vertices_.push_back({x0, y0, u0, v0, r, g, b, a});
    vertices_.push_back({x1, y1, u1, v1, r, g, b, a});
    vertices_.push_back({x2, y2, u2, v2, r, g, b, a});
}

// ============================================================================
// drawRect
// ============================================================================

void Batch2D::drawRect(float x, float y, float w, float h, const Color& color) {
    setTexture(whiteTexture_);

    auto n = color.normalized();
    pushQuad(
        x,     y,     0.0f, 0.0f,
        x + w, y,     1.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y + h, 0.0f, 1.0f,
        n.r, n.g, n.b, n.a
    );
}

// ============================================================================
// drawRectRounded
// ============================================================================

void Batch2D::drawRectRounded(float x, float y, float w, float h,
                              float roundness, int segments, const Color& color)
{
    if (roundness <= 0.0f || segments <= 0) {
        drawRect(x, y, w, h, color);
        return;
    }

    setTexture(whiteTexture_);
    auto n = color.normalized();

    float radius = std::min(roundness * std::min(w, h) * 0.5f, std::min(w, h) * 0.5f);
    if (segments < 4) segments = 4;

    // Center cross rects
    drawRect(x + radius, y, w - 2 * radius, h, color);
    drawRect(x, y + radius, radius, h - 2 * radius, color);
    drawRect(x + w - radius, y + radius, radius, h - 2 * radius, color);

    // Corner arcs (fan triangles)
    float cx, cy;
    float startAngle;
    // top-left
    cx = x + radius; cy = y + radius; startAngle = 3.14159265f;
    for (int i = 0; i < segments; ++i) {
        float a0 = startAngle + (3.14159265f / 2.0f) * (float(i) / float(segments));
        float a1 = startAngle + (3.14159265f / 2.0f) * (float(i + 1) / float(segments));
        pushTriangle(
            cx, cy, 0.5f, 0.5f,
            cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0.5f, 0.5f,
            cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0.5f, 0.5f,
            n.r, n.g, n.b, n.a
        );
    }
    // top-right
    cx = x + w - radius; cy = y + radius; startAngle = -3.14159265f / 2.0f;
    for (int i = 0; i < segments; ++i) {
        float a0 = startAngle + (3.14159265f / 2.0f) * (float(i) / float(segments));
        float a1 = startAngle + (3.14159265f / 2.0f) * (float(i + 1) / float(segments));
        pushTriangle(
            cx, cy, 0.5f, 0.5f,
            cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0.5f, 0.5f,
            cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0.5f, 0.5f,
            n.r, n.g, n.b, n.a
        );
    }
    // bottom-right
    cx = x + w - radius; cy = y + h - radius; startAngle = 0.0f;
    for (int i = 0; i < segments; ++i) {
        float a0 = startAngle + (3.14159265f / 2.0f) * (float(i) / float(segments));
        float a1 = startAngle + (3.14159265f / 2.0f) * (float(i + 1) / float(segments));
        pushTriangle(
            cx, cy, 0.5f, 0.5f,
            cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0.5f, 0.5f,
            cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0.5f, 0.5f,
            n.r, n.g, n.b, n.a
        );
    }
    // bottom-left
    cx = x + radius; cy = y + h - radius; startAngle = 3.14159265f / 2.0f;
    for (int i = 0; i < segments; ++i) {
        float a0 = startAngle + (3.14159265f / 2.0f) * (float(i) / float(segments));
        float a1 = startAngle + (3.14159265f / 2.0f) * (float(i + 1) / float(segments));
        pushTriangle(
            cx, cy, 0.5f, 0.5f,
            cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0.5f, 0.5f,
            cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0.5f, 0.5f,
            n.r, n.g, n.b, n.a
        );
    }
}

// ============================================================================
// drawRectLines
// ============================================================================

void Batch2D::drawRectLines(float x, float y, float w, float h, const Color& color) {
    drawRectLinesEx(x, y, w, h, 1.0f, color);
}

void Batch2D::drawRectLinesEx(float x, float y, float w, float h,
                               float lineThick, const Color& color)
{
    // Top
    drawRect(x, y, w, lineThick, color);
    // Bottom
    drawRect(x, y + h - lineThick, w, lineThick, color);
    // Left
    drawRect(x, y + lineThick, lineThick, h - 2 * lineThick, color);
    // Right
    drawRect(x + w - lineThick, y + lineThick, lineThick, h - 2 * lineThick, color);
}

// ============================================================================
// drawRectRoundedLines
// ============================================================================

void Batch2D::drawRectRoundedLines(float x, float y, float w, float h,
                                    float roundness, int segments,
                                    float lineThick, const Color& color)
{
    if (roundness <= 0.0f || segments <= 0) {
        drawRectLinesEx(x, y, w, h, lineThick, color);
        return;
    }

    // Approximate by drawing outer rounded rect minus inner
    // For simplicity, draw line segments around the rounded rectangle
    setTexture(whiteTexture_);
    auto n = color.normalized();

    float radius = std::min(roundness * std::min(w, h) * 0.5f, std::min(w, h) * 0.5f);
    if (segments < 4) segments = 4;

    float innerRadius = std::max(radius - lineThick, 0.0f);

    // Straight edges
    // Top
    drawRect(x + radius, y, w - 2 * radius, lineThick, color);
    // Bottom
    drawRect(x + radius, y + h - lineThick, w - 2 * radius, lineThick, color);
    // Left
    drawRect(x, y + radius, lineThick, h - 2 * radius, color);
    // Right
    drawRect(x + w - lineThick, y + radius, lineThick, h - 2 * radius, color);

    // Corner arcs (as thick arcs using two triangles per segment)
    auto drawCornerArc = [&](float cx, float cy, float startAngle) {
        for (int i = 0; i < segments; ++i) {
            float a0 = startAngle + (3.14159265f / 2.0f) * (float(i) / float(segments));
            float a1 = startAngle + (3.14159265f / 2.0f) * (float(i + 1) / float(segments));
            float c0 = std::cos(a0), s0 = std::sin(a0);
            float c1 = std::cos(a1), s1 = std::sin(a1);

            // Outer and inner rings
            float ox0 = cx + c0 * radius,       oy0 = cy + s0 * radius;
            float ox1 = cx + c1 * radius,       oy1 = cy + s1 * radius;
            float ix0 = cx + c0 * innerRadius,  iy0 = cy + s0 * innerRadius;
            float ix1 = cx + c1 * innerRadius,  iy1 = cy + s1 * innerRadius;

            pushTriangle(ox0, oy0, 0.5f, 0.5f,
                         ox1, oy1, 0.5f, 0.5f,
                         ix0, iy0, 0.5f, 0.5f,
                         n.r, n.g, n.b, n.a);
            pushTriangle(ix0, iy0, 0.5f, 0.5f,
                         ox1, oy1, 0.5f, 0.5f,
                         ix1, iy1, 0.5f, 0.5f,
                         n.r, n.g, n.b, n.a);
        }
    };

    drawCornerArc(x + radius,     y + radius,         3.14159265f);          // top-left
    drawCornerArc(x + w - radius, y + radius,        -3.14159265f / 2.0f);   // top-right
    drawCornerArc(x + w - radius, y + h - radius,     0.0f);                 // bottom-right
    drawCornerArc(x + radius,     y + h - radius,     3.14159265f / 2.0f);   // bottom-left
}

// ============================================================================
// drawTexture
// ============================================================================

void Batch2D::drawTexture(const ITexture* texture,
                          Rect src, Rect dst,
                          Vec2 origin, float rotation,
                          const Color& tint)
{
    if (!texture || !texture->isValid()) return;

    setTexture(texture->nativeHandle());

    float tw = static_cast<float>(texture->width());
    float th = static_cast<float>(texture->height());

    // Handle negative source dimensions (flip)
    bool flipX = src.w < 0;
    bool flipY = src.h < 0;
    float srcW = std::abs(src.w);
    float srcH = std::abs(src.h);

    // UV coordinates
    float u0 = src.x / tw;
    float v0 = src.y / th;
    float u1 = (src.x + srcW) / tw;
    float v1 = (src.y + srcH) / th;

    if (flipX) std::swap(u0, u1);
    if (flipY) std::swap(v0, v1);

    auto n = tint.normalized();

    if (rotation == 0.0f) {
        // Fast path — no rotation
        float x0 = dst.x - origin.x;
        float y0 = dst.y - origin.y;
        float x1 = x0 + dst.w;
        float y1 = y0 + dst.h;

        pushQuad(
            x0, y0, u0, v0,
            x1, y0, u1, v0,
            x1, y1, u1, v1,
            x0, y1, u0, v1,
            n.r, n.g, n.b, n.a
        );
    } else {
        // Rotation around origin
        float rad = rotation * (3.14159265f / 180.0f);
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);

        // Quad corners relative to origin
        float px0 = -origin.x,        py0 = -origin.y;
        float px1 = dst.w - origin.x,  py1 = -origin.y;
        float px2 = dst.w - origin.x,  py2 = dst.h - origin.y;
        float px3 = -origin.x,        py3 = dst.h - origin.y;

        auto rot = [&](float& rx, float& ry, float lx, float ly) {
            rx = dst.x + lx * cosR - ly * sinR;
            ry = dst.y + lx * sinR + ly * cosR;
        };

        float rx0, ry0, rx1, ry1, rx2, ry2, rx3, ry3;
        rot(rx0, ry0, px0, py0);
        rot(rx1, ry1, px1, py1);
        rot(rx2, ry2, px2, py2);
        rot(rx3, ry3, px3, py3);

        pushQuad(
            rx0, ry0, u0, v0,
            rx1, ry1, u1, v0,
            rx2, ry2, u1, v1,
            rx3, ry3, u0, v1,
            n.r, n.g, n.b, n.a
        );
    }
}

// ============================================================================
// drawTextureRaw
// ============================================================================

void Batch2D::drawTextureRaw(std::uintptr_t nativeHandle, int texW, int texH,
                             Rect src, Rect dst,
                             Vec2 origin, float rotation,
                             const Color& tint)
{
    if (nativeHandle == 0) return;

    setTexture(nativeHandle);

    float tw = static_cast<float>(texW);
    float th = static_cast<float>(texH);

    bool flipX = src.w < 0;
    bool flipY = src.h < 0;
    float srcW = std::abs(src.w);
    float srcH = std::abs(src.h);

    float u0 = src.x / tw;
    float v0 = src.y / th;
    float u1 = (src.x + srcW) / tw;
    float v1 = (src.y + srcH) / th;

    if (flipX) std::swap(u0, u1);
    if (flipY) std::swap(v0, v1);

    auto n = tint.normalized();

    if (rotation == 0.0f) {
        float x0 = dst.x - origin.x;
        float y0 = dst.y - origin.y;
        float x1 = x0 + dst.w;
        float y1 = y0 + dst.h;

        pushQuad(
            x0, y0, u0, v0,
            x1, y0, u1, v0,
            x1, y1, u1, v1,
            x0, y1, u0, v1,
            n.r, n.g, n.b, n.a
        );
    } else {
        float rad = rotation * (3.14159265f / 180.0f);
        float cosR = std::cos(rad);
        float sinR = std::sin(rad);

        float px0 = -origin.x,        py0 = -origin.y;
        float px1 = dst.w - origin.x,  py1 = -origin.y;
        float px2 = dst.w - origin.x,  py2 = dst.h - origin.y;
        float px3 = -origin.x,        py3 = dst.h - origin.y;

        auto rot = [&](float& rx, float& ry, float lx, float ly) {
            rx = dst.x + lx * cosR - ly * sinR;
            ry = dst.y + lx * sinR + ly * cosR;
        };

        float rx0, ry0, rx1, ry1, rx2, ry2, rx3, ry3;
        rot(rx0, ry0, px0, py0);
        rot(rx1, ry1, px1, py1);
        rot(rx2, ry2, px2, py2);
        rot(rx3, ry3, px3, py3);

        pushQuad(
            rx0, ry0, u0, v0,
            rx1, ry1, u1, v0,
            rx2, ry2, u1, v1,
            rx3, ry3, u0, v1,
            n.r, n.g, n.b, n.a
        );
    }
}

// ============================================================================
// drawCircle
// ============================================================================

void Batch2D::drawCircle(float cx, float cy, float radius, const Color& color, int segments) {
    setTexture(whiteTexture_);
    auto n = color.normalized();

    float angleStep = 2.0f * 3.14159265f / static_cast<float>(segments);

    for (int i = 0; i < segments; ++i) {
        float a0 = angleStep * static_cast<float>(i);
        float a1 = angleStep * static_cast<float>(i + 1);

        pushTriangle(
            cx, cy, 0.5f, 0.5f,
            cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, 0.5f, 0.5f,
            cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0.5f, 0.5f,
            n.r, n.g, n.b, n.a
        );
    }
}

// ============================================================================
// drawLine
// ============================================================================

void Batch2D::drawLine(float x1, float y1, float x2, float y2,
                       float thickness, const Color& color)
{
    setTexture(whiteTexture_);
    auto n = color.normalized();

    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.0001f) return;

    float nx_ = -(dy / len) * thickness * 0.5f;
    float ny_ =  (dx / len) * thickness * 0.5f;

    pushQuad(
        x1 + nx_, y1 + ny_, 0.0f, 0.0f,
        x2 + nx_, y2 + ny_, 1.0f, 0.0f,
        x2 - nx_, y2 - ny_, 1.0f, 1.0f,
        x1 - nx_, y1 - ny_, 0.0f, 1.0f,
        n.r, n.g, n.b, n.a
    );
}

// ============================================================================
// Text (convenience)
// ============================================================================

void Batch2D::drawText(const GLFont* font, const std::string& text,
                       float x, float y, float size, const Color& color)
{
    if (!available_) return;

#if RAYFLOW_HAS_DX11
    if (backend_ == Backend::DirectX11) {
        drawTextDX11(text, x, y, size, color);
        return;
    }
#endif

    if (backend_ != Backend::OpenGL) return;

    if (!font || !font->isValid()) {
        if (auto* def = GLFont::defaultFont()) {
            font = def;
        } else {
            return;
        }
    }

    // Text drawing flushes the batch (different shader), then resumes
    flush();
    font->drawText(text, x, y, size, color);
}

void Batch2D::drawText(const std::string& text, float x, float y,
                       float size, const Color& color)
{
    if (!available_) return;

#if RAYFLOW_HAS_DX11
    if (backend_ == Backend::DirectX11) {
        drawTextDX11(text, x, y, size, color);
        return;
    }
#endif

    if (backend_ != Backend::OpenGL) return;
    drawText(GLFont::defaultFont(), text, x, y, size, color);
}

float Batch2D::measureText(const GLFont* font, const std::string& text, float size) {
    if (!available_) return 0.0f;

#if RAYFLOW_HAS_DX11
    if (backend_ == Backend::DirectX11)
        return measureTextDX11(text, size);
#endif

    if (backend_ != Backend::OpenGL) return 0.0f;
    if (!font || !font->isValid()) {
        font = GLFont::defaultFont();
        if (!font) return 0.0f;
    }
    return font->measureText(text, size);
}

float Batch2D::measureText(const std::string& text, float size) {
    if (!available_) return 0.0f;

#if RAYFLOW_HAS_DX11
    if (backend_ == Backend::DirectX11)
        return measureTextDX11(text, size);
#endif

    if (backend_ != Backend::OpenGL) return 0.0f;
    return measureText(GLFont::defaultFont(), text, size);
}

// ============================================================================
// DirectX 11 backend implementation
// ============================================================================

#if RAYFLOW_HAS_DX11

void Batch2D::ensureInitialisedDX11() {
    dxDevice_ = static_cast<DX11RenderDevice*>(resources::render_device());
    if (!dxDevice_) {
        TraceLog(LOG_ERROR, "[Batch2D] DX11: No render device available");
        initialised_ = true;
        available_ = false;
        return;
    }

    auto* d3dDevice  = dxDevice_->device();
    auto* d3dContext = dxDevice_->context();

    // --- Compile shader ---
    dxShader_ = new DX11Shader(dxDevice_);
    if (!dxShader_->loadFromFiles("shaders/batch2d.vs", "shaders/batch2d.fs")) {
        TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to compile batch shader");
        delete dxShader_; dxShader_ = nullptr;
        initialised_ = true;
        available_ = false;
        return;
    }

    // --- Create custom input layout for Batch2D vertex format ---
    // The default DX11Shader input layout is for meshes (pos3+uv2+uv2+normal3+color_ubyte4).
    // Batch2D uses a different interleaved format: pos2(float)+uv2(float)+color4(float).
    // We need to create a custom layout. To do this, we need the VS bytecode.
    // Since DX11Shader already created one internally, we'll create our own
    // and store it separately.
    // For now, the shader's auto-generated layout won't match. We'll compile
    // the VS again just to get the bytecode for CreateInputLayout.
    {
        // Load the VS source to get bytecode for input layout creation
        std::string vsSrc = resources::load_text("shaders/hlsl/batch2d.vs.hlsl");
        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompile(
            vsSrc.data(), vsSrc.size(),
            "batch2d.vs.hlsl", nullptr, nullptr,
            "VSMain", "vs_5_0",
            D3DCOMPILE_ENABLE_STRICTNESS, 0,
            &vsBlob, &errorBlob);

        if (FAILED(hr)) {
            TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to compile VS for input layout: %s",
                     errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown");
            delete dxShader_; dxShader_ = nullptr;
            initialised_ = true;
            available_ = false;
            return;
        }

        D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Vertex, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = d3dDevice->CreateInputLayout(
            layoutDesc, _countof(layoutDesc),
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            &dxInputLayout_);

        if (FAILED(hr)) {
            TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to create input layout (HRESULT 0x%08X)", hr);
            delete dxShader_; dxShader_ = nullptr;
            initialised_ = true;
            available_ = false;
            return;
        }
    }

    // --- Create dynamic vertex buffer ---
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth      = kMaxVertices * sizeof(Vertex);
        desc.Usage           = D3D11_USAGE_DYNAMIC;
        desc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = d3dDevice->CreateBuffer(&desc, nullptr, &dxVertexBuffer_);
        if (FAILED(hr)) {
            TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to create vertex buffer (HRESULT 0x%08X)", hr);
            delete dxShader_; dxShader_ = nullptr;
            initialised_ = true;
            available_ = false;
            return;
        }
    }

    // --- Create 1x1 white texture ---
    {
        dxWhiteTexture_ = new DX11Texture(dxDevice_);
        std::uint8_t white[] = {255, 255, 255, 255};
        if (!dxWhiteTexture_->loadFromMemory(white, 1, 1, 4)) {
            TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to create white texture");
            delete dxShader_; dxShader_ = nullptr;
            initialised_ = true;
            available_ = false;
            return;
        }
        whiteTexture_ = dxWhiteTexture_->nativeHandle();
    }

    // --- Create sampler state ---
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;

        HRESULT hr = d3dDevice->CreateSamplerState(&sd, &dxSampler_);
        if (FAILED(hr)) {
            TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to create sampler state (HRESULT 0x%08X)", hr);
            delete dxShader_; dxShader_ = nullptr;
            initialised_ = true;
            available_ = false;
            return;
        }
    }

    vertices_.reserve(4096);

    initialised_ = true;
    available_ = true;
    TraceLog(LOG_INFO, "[Batch2D] Initialised DirectX 11 (max %d vertices)", kMaxVertices);
}

void Batch2D::flushDX11() {
    if (vertices_.empty()) return;

    auto* ctx = dxDevice_->context();

    int count = static_cast<int>(vertices_.size());
    if (count > kMaxVertices) count = kMaxVertices;

    // Upload vertex data
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = ctx->Map(dxVertexBuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        vertices_.clear();
        return;
    }
    std::memcpy(mapped.pData, vertices_.data(), count * sizeof(Vertex));
    ctx->Unmap(dxVertexBuffer_, 0);

    // Set projection uniform before bind() — bind() flushes constant buffers
    dxShader_->setMat4("uProjection", projection_);
    dxShader_->bind();

    // Bind input layout
    ctx->IASetInputLayout(dxInputLayout_);

    // Bind vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &dxVertexBuffer_, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Bind texture and sampler
    auto* srv = reinterpret_cast<ID3D11ShaderResourceView*>(currentTexture_);
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->PSSetSamplers(0, 1, &dxSampler_);

    // Draw
    ctx->Draw(static_cast<UINT>(count), 0);

    vertices_.clear();
}

// ============================================================================
// DX11 font atlas (baked via stb_truetype, rendered through Batch2D pipeline)
// ============================================================================

bool Batch2D::ensureDX11Font() {
    if (dxFontLoaded_) return (dxFontAtlas_ != nullptr);

    dxFontLoaded_ = true; // prevent repeated attempts

    // Load TTF from system fonts (same list as GLFont::defaultFont)
    const char* systemFonts[] = {
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };

    std::vector<std::uint8_t> ttfData;
    for (const char* path : systemFonts) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;
        auto sz = file.tellg();
        file.seekg(0);
        ttfData.resize(static_cast<size_t>(sz));
        file.read(reinterpret_cast<char*>(ttfData.data()), sz);
        TraceLog(LOG_INFO, "[Batch2D] DX11 font loaded: %s", path);
        break;
    }

    if (ttfData.empty()) {
        TraceLog(LOG_WARNING, "[Batch2D] DX11: No system font found");
        return false;
    }

    // Init stb_truetype
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, ttfData.data(),
                        stbtt_GetFontOffsetForIndex(ttfData.data(), 0))) {
        TraceLog(LOG_WARNING, "[Batch2D] DX11: stbtt_InitFont failed");
        return false;
    }

    constexpr float kBakeSize = 20.0f;
    dxFontBakedSize_ = kBakeSize;

    float scale = stbtt_ScaleForPixelHeight(&fontInfo, kBakeSize);

    int iAscent, iDescent, iLineGap;
    stbtt_GetFontVMetrics(&fontInfo, &iAscent, &iDescent, &iLineGap);
    dxFontAscent_ = iAscent * scale;

    // Pack the atlas
    int atlasW = 512, atlasH = 512;
    std::vector<std::uint8_t> bitmap;
    stbtt_pack_context packCtx;
    stbtt_packedchar packedChars[kCharCount];

    for (int attempt = 0; attempt < 4; ++attempt) {
        bitmap.assign(atlasW * atlasH, 0);
        if (!stbtt_PackBegin(&packCtx, bitmap.data(), atlasW, atlasH, 0, 1, nullptr)) {
            TraceLog(LOG_WARNING, "[Batch2D] DX11: stbtt_PackBegin failed");
            return false;
        }
        stbtt_PackSetOversampling(&packCtx, 2, 2);
        int result = stbtt_PackFontRange(&packCtx, ttfData.data(), 0, kBakeSize,
                                         kFirstChar, kCharCount, packedChars);
        stbtt_PackEnd(&packCtx);
        if (result != 0) break;
        if (atlasW <= atlasH) atlasW *= 2; else atlasH *= 2;
    }

    // Fill glyph info
    float invW = 1.0f / static_cast<float>(atlasW);
    float invH = 1.0f / static_cast<float>(atlasH);
    for (int i = 0; i < kCharCount; ++i) {
        const auto& pc = packedChars[i];
        auto& g = dxGlyphs_[i];
        g.x0 = pc.xoff;  g.y0 = pc.yoff;
        g.x1 = pc.xoff2; g.y1 = pc.yoff2;
        g.s0 = pc.x0 * invW; g.t0 = pc.y0 * invH;
        g.s1 = pc.x1 * invW; g.t1 = pc.y1 * invH;
        g.xAdvance = pc.xadvance;
    }

    // Convert R8 bitmap → RGBA with (255,255,255,fontAlpha)
    // DX11Texture::loadFromMemory with channels=1 sets alpha=255 (not source value),
    // so we must prepare RGBA ourselves with the glyph value in the alpha channel.
    // This way: tex * color = (1*r, 1*g, 1*b, fontAlpha*a) — correct alpha blending.
    std::vector<std::uint8_t> rgba(atlasW * atlasH * 4);
    for (int i = 0; i < atlasW * atlasH; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }

    // Upload as DX11Texture (channels=4 so no conversion)
    auto* tex = new DX11Texture(dxDevice_);
    if (!tex->loadFromMemory(rgba.data(), atlasW, atlasH, 4)) {
        TraceLog(LOG_ERROR, "[Batch2D] DX11: Failed to upload font atlas");
        delete tex;
        return false;
    }

    dxFontAtlas_ = tex;
    TraceLog(LOG_INFO, "[Batch2D] DX11 font atlas created: %dx%d", atlasW, atlasH);
    return true;
}

void Batch2D::drawTextDX11(const std::string& text, float x, float y,
                           float size, const Color& color) {
    if (text.empty() || !ensureDX11Font()) return;

    float scale = size / dxFontBakedSize_;
    auto n = color.normalized();

    // Bind the font atlas texture
    setTexture(dxFontAtlas_->nativeHandle());

    float cursorX = x;
    float cursorY = y + dxFontAscent_ * scale; // baseline offset

    for (char ch : text) {
        int ci = static_cast<int>(ch) - kFirstChar;
        if (ci < 0 || ci >= kCharCount) {
            cursorX += dxGlyphs_[0].xAdvance * scale;
            continue;
        }

        const auto& g = dxGlyphs_[ci];

        float qx0 = cursorX + g.x0 * scale;
        float qy0 = cursorY + g.y0 * scale;
        float qx1 = cursorX + g.x1 * scale;
        float qy1 = cursorY + g.y1 * scale;

        pushQuad(
            qx0, qy0, g.s0, g.t0,
            qx1, qy0, g.s1, g.t0,
            qx1, qy1, g.s1, g.t1,
            qx0, qy1, g.s0, g.t1,
            n.r, n.g, n.b, n.a
        );

        cursorX += g.xAdvance * scale;
    }
}

float Batch2D::measureTextDX11(const std::string& text, float size) const {
    if (text.empty() || !dxFontLoaded_ || !dxFontAtlas_) return 0.0f;

    float scale = size / dxFontBakedSize_;
    float width = 0.0f;

    for (char ch : text) {
        int ci = static_cast<int>(ch) - kFirstChar;
        if (ci < 0 || ci >= kCharCount) {
            width += dxGlyphs_[0].xAdvance * scale;
            continue;
        }
        width += dxGlyphs_[ci].xAdvance * scale;
    }
    return width;
}

void Batch2D::destroyDX11() {
    delete dxShader_;       dxShader_ = nullptr;
    delete dxWhiteTexture_; dxWhiteTexture_ = nullptr;
    delete dxFontAtlas_;    dxFontAtlas_ = nullptr;

    if (dxVertexBuffer_) { dxVertexBuffer_->Release(); dxVertexBuffer_ = nullptr; }
    if (dxSampler_)      { dxSampler_->Release();      dxSampler_ = nullptr; }
    if (dxInputLayout_)  { dxInputLayout_->Release();   dxInputLayout_ = nullptr; }

    dxDevice_ = nullptr;
    dxFontLoaded_ = false;
}

#endif // RAYFLOW_HAS_DX11

} // namespace rf
