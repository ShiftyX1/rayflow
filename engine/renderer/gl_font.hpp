#pragma once

// =============================================================================
// GLFont — Font atlas + text rendering via stb_truetype + OpenGL
//
// Phase 3: Replaces raylib Font / LoadFontEx / DrawText / MeasureText.
// Loads a TTF via stb_truetype, bakes a bitmap atlas, and provides
// drawText() / measureText() that use a textured quad batch internally.
// =============================================================================

#include "engine/core/math_types.hpp"
#include "gl_shader.hpp"

#include <glad/gl.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace rf {

class GLFont {
public:
    GLFont() = default;
    ~GLFont();

    // Non-copyable, movable
    GLFont(const GLFont&) = delete;
    GLFont& operator=(const GLFont&) = delete;
    GLFont(GLFont&& other) noexcept;
    GLFont& operator=(GLFont&& other) noexcept;

    // ----- Loading -----

    /// Load a TTF font from file and bake an atlas at the given pixel size.
    /// Bakes ASCII range 32..126 by default.
    /// @param path     Path to a .ttf file (resolved through resources / VFS).
    /// @param fontSize Pixel height of the rasterised glyphs.
    /// @return true on success.
    bool loadFromFile(const std::string& path, float fontSize = 20.0f);

    /// Load from raw TTF data already in memory.
    bool loadFromMemory(const std::uint8_t* data, int dataSize, float fontSize = 20.0f);

    /// Destroy atlas texture and free glyph data.
    void destroy();

    // ----- Text rendering -----

    /// Draw a string at pixel coordinates (screen-space, top-left origin).
    /// Assumes an orthographic projection is already set (see setProjection).
    /// @param text   UTF-8 string (only ASCII chars that were baked will render).
    /// @param x, y   Top-left corner of the first glyph.
    /// @param size   Desired pixel size (scales relative to baked fontSize).
    /// @param color  Tint color.
    void drawText(const std::string& text, float x, float y,
                  float size, const Color& color = Color::White()) const;

    /// Measure the width in pixels of a string at a given size.
    float measureText(const std::string& text, float size) const;

    /// Returns the line height at a given rendering size.
    float lineHeight(float size) const;

    // ----- Projection -----

    /// Set the orthographic projection used by drawText().
    /// Typically called once per frame: setProjection(screenWidth, screenHeight).
    static void setProjection(float width, float height);

    // ----- Queries -----

    bool isValid() const { return atlasTexture_ != 0; }
    float bakedSize() const { return fontSize_; }
    GLuint atlasTextureId() const { return atlasTexture_; }

    // ----- Default font -----

    /// Get or create a built-in default font (baked from embedded data).
    /// Returns nullptr if creation fails.
    static GLFont* defaultFont();

private:
    // Per-glyph metrics
    struct GlyphInfo {
        float x0, y0, x1, y1;   // quad corners relative to cursor (pixels at baked size)
        float s0, t0, s1, t1;   // UV coordinates in atlas [0..1]
        float xAdvance;          // horizontal advance (pixels at baked size)
    };

    bool bakeAtlas(const std::uint8_t* ttfData, int ttfLen);

    static GLShader& textShader();

    float fontSize_{0.0f};
    int atlasWidth_{0};
    int atlasHeight_{0};
    GLuint atlasTexture_{0};

    // Glyph info indexed by character code (only ASCII 32..126)
    static constexpr int kFirstChar = 32;
    static constexpr int kCharCount = 95; // 32..126 inclusive
    GlyphInfo glyphs_[kCharCount]{};
    float ascent_{0.0f};
    float descent_{0.0f};
    float lineGap_{0.0f};

    // Shared text shader + projection
    static GLShader  sTextShader_;
    static Mat4      sProjection_;
    static bool      sShaderInitialised_;

    // Shared quad VAO/VBO for text rendering (dynamic)
    static GLuint sVAO_;
    static GLuint sVBO_;
    static bool   sQuadInitialised_;

    static void ensureQuad();
};

} // namespace rf
