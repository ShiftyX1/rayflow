#pragma once

// =============================================================================
// Batch2D — Batched 2D quad/sprite/shape renderer
//
// Phase 3: Replaces raylib DrawRectangle / DrawTexturePro / DrawTextureRec /
//          DrawCircleV / DrawLineV / DrawRectangleLines etc.
//
// Collects quads into a single VBO and draws everything in one (or a few)
// draw calls per flush(). Texture switches trigger an automatic flush.
//
// Usage (per frame):
//   Batch2D::instance().begin(screenWidth, screenHeight);
//   batch.drawRect(10, 10, 100, 30, Color::Red());
//   batch.drawTexture(tex, srcRect, dstRect, Color::White());
//   batch.drawText(font, "Hello", 10, 50, 16, Color::White());
//   batch.end();   // flushes remaining geometry
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "gl_shader.hpp"

#include <glad/gl.h>

#include <vector>

namespace rf {

class GLTexture;
class GLFont;

class RAYFLOW_CLIENT_API Batch2D {
public:
    /// Singleton accessor (only one batch renderer per process).
    static Batch2D& instance();

    // ----- Frame lifecycle -----

    /// Begin a new 2D rendering frame. Sets up orthographic projection.
    /// Must be called before any draw* methods.
    void begin(int screenWidth, int screenHeight);

    /// Flush any remaining geometry and restore GL state.
    void end();

    // ----- Rectangles -----

    /// Draw a filled rectangle.
    void drawRect(float x, float y, float w, float h, const Color& color);

    /// Draw a filled rectangle with rounded corners.
    /// @param roundness 0.0 = sharp, 1.0 = maximally rounded.
    /// @param segments  Number of arc segments per corner.
    void drawRectRounded(float x, float y, float w, float h,
                         float roundness, int segments, const Color& color);

    /// Draw a rectangle outline (1-pixel width).
    void drawRectLines(float x, float y, float w, float h, const Color& color);

    /// Draw a rectangle outline with explicit line thickness.
    void drawRectLinesEx(float x, float y, float w, float h,
                         float lineThick, const Color& color);

    /// Draw a rounded rectangle outline.
    void drawRectRoundedLines(float x, float y, float w, float h,
                              float roundness, int segments,
                              float lineThick, const Color& color);

    // ----- Textures / Sprites -----

    /// Draw a textured quad (source rect → destination rect) with tint.
    /// @param texture  Pointer to bound GLTexture.
    /// @param src      Source rectangle in texture-pixel coords.
    /// @param dst      Destination rectangle in screen-pixel coords.
    /// @param origin   Pivot for rotation (relative to dst).
    /// @param rotation Rotation in degrees.
    /// @param tint     Multiplicative tint.
    void drawTexture(const GLTexture* texture,
                     Rect src, Rect dst,
                     Vec2 origin = {0,0}, float rotation = 0.0f,
                     const Color& tint = Color::White());

    /// Draw a textured quad from a raw OpenGL texture ID.
    /// @param texId    OpenGL texture name.
    /// @param texW     Texture width in pixels.
    /// @param texH     Texture height in pixels.
    void drawTextureRaw(GLuint texId, int texW, int texH,
                        Rect src, Rect dst,
                        Vec2 origin = {0,0}, float rotation = 0.0f,
                        const Color& tint = Color::White());

    // ----- Circles -----

    /// Draw a filled circle.
    void drawCircle(float cx, float cy, float radius, const Color& color, int segments = 24);

    // ----- Lines -----

    /// Draw a line between two points with given thickness.
    void drawLine(float x1, float y1, float x2, float y2,
                  float thickness, const Color& color);

    // ----- Text (convenience wrappers) -----

    /// Draw text using a GLFont (forwards to GLFont::drawText).
    void drawText(const GLFont* font, const std::string& text,
                  float x, float y, float size,
                  const Color& color = Color::White());

    /// Draw text using default system font.
    void drawText(const std::string& text, float x, float y, float size,
                  const Color& color = Color::White());

    /// Measure text width using a GLFont.
    float measureText(const GLFont* font, const std::string& text, float size);

    /// Measure text width using default font.
    float measureText(const std::string& text, float size);

private:
    Batch2D() = default;
    ~Batch2D();

    Batch2D(const Batch2D&) = delete;
    Batch2D& operator=(const Batch2D&) = delete;

    // Internal vertex: pos(2) + uv(2) + color(4) = 8 floats
    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    void ensureInitialised();
    void flush();
    void setTexture(GLuint texId);
    void pushQuad(float x0, float y0, float u0, float v0,
                  float x1, float y1, float u1, float v1,
                  float x2, float y2, float u2, float v2,
                  float x3, float y3, float u3, float v3,
                  float r, float g, float b, float a);
    void pushTriangle(float x0, float y0, float u0, float v0,
                      float x1, float y1, float u1, float v1,
                      float x2, float y2, float u2, float v2,
                      float r, float g, float b, float a);

    static constexpr int kMaxVertices = 65536;

    bool initialised_{false};
    bool inFrame_{false};

    GLuint vao_{0};
    GLuint vbo_{0};
    GLShader shader_;
    GLuint whiteTexture_{0}; // 1x1 white pixel for untextured quads
    Mat4 projection_{1.0f};

    // Current batch state
    GLuint currentTexture_{0};
    std::vector<Vertex> vertices_;
};

} // namespace rf
