#include "batch_2d.hpp"

#include "gl_texture.hpp"
#include "gl_font.hpp"
#include "engine/core/logging.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>

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
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (whiteTexture_) glDeleteTextures(1, &whiteTexture_);
    shader_.destroy();
}

// ============================================================================
// Lazy init
// ============================================================================

void Batch2D::ensureInitialised() {
    if (initialised_) return;

    // Compile shader
    if (!shader_.loadFromSource(kBatch2DVertSrc, kBatch2DFragSrc)) {
        TraceLog(LOG_ERROR, "[Batch2D] Failed to compile batch shader");
        return;
    }

    // Create VAO/VBO
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

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
    glGenTextures(1, &whiteTexture_);
    glBindTexture(GL_TEXTURE_2D, whiteTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    vertices_.reserve(4096);

    initialised_ = true;
    TraceLog(LOG_INFO, "[Batch2D] Initialised (max %d vertices)", kMaxVertices);
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void Batch2D::begin(int screenWidth, int screenHeight) {
    ensureInitialised();

    projection_ = glm::ortho(0.0f, static_cast<float>(screenWidth),
                              static_cast<float>(screenHeight), 0.0f,
                              -1.0f, 1.0f);

    // Also set projection for GLFont text rendering
    GLFont::setProjection(static_cast<float>(screenWidth),
                          static_cast<float>(screenHeight));

    currentTexture_ = whiteTexture_;
    vertices_.clear();
    inFrame_ = true;

    // Setup GL state for 2D
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Batch2D::end() {
    if (!inFrame_) return;
    flush();
    inFrame_ = false;

    // Restore common state
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// Flush
// ============================================================================

void Batch2D::flush() {
    if (vertices_.empty()) return;

    shader_.bind();
    shader_.setMat4("uProjection", projection_);
    shader_.setInt("uTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, currentTexture_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    int count = static_cast<int>(vertices_.size());
    if (count > kMaxVertices) count = kMaxVertices;

    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    count * sizeof(Vertex),
                    vertices_.data());

    glDrawArrays(GL_TRIANGLES, 0, count);

    glBindVertexArray(0);
    GLShader::unbind();

    vertices_.clear();
}

void Batch2D::setTexture(GLuint texId) {
    if (texId == 0) texId = whiteTexture_;
    if (texId != currentTexture_) {
        flush();
        currentTexture_ = texId;
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

void Batch2D::drawTexture(const GLTexture* texture,
                          Rect src, Rect dst,
                          Vec2 origin, float rotation,
                          const Color& tint)
{
    if (!texture || !texture->isValid()) return;

    setTexture(texture->id());

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

void Batch2D::drawTextureRaw(GLuint texId, int texW, int texH,
                             Rect src, Rect dst,
                             Vec2 origin, float rotation,
                             const Color& tint)
{
    if (texId == 0) return;

    setTexture(texId);

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
    if (!font || !font->isValid()) {
        // Fallback to default font
        if (auto* def = GLFont::defaultFont()) {
            font = def;
        } else {
            return;
        }
    }

    // Text drawing flushes the batch (different shader), then resumes
    flush();
    font->drawText(text, x, y, size, color);

    // Re-bind batch state
    // (next draw* call will rebind via setTexture)
}

void Batch2D::drawText(const std::string& text, float x, float y,
                       float size, const Color& color)
{
    drawText(GLFont::defaultFont(), text, x, y, size, color);
}

float Batch2D::measureText(const GLFont* font, const std::string& text, float size) {
    if (!font || !font->isValid()) {
        font = GLFont::defaultFont();
        if (!font) return 0.0f;
    }
    return font->measureText(text, size);
}

float Batch2D::measureText(const std::string& text, float size) {
    return measureText(GLFont::defaultFont(), text, size);
}

} // namespace rf
