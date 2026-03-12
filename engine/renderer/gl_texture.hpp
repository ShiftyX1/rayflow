#pragma once

// =============================================================================
// GLTexture — OpenGL texture wrapper
//
// Phase 2: Replaces raylib Texture2D / LoadTexture / LoadTextureCubemap etc.
// Loads images via stb_image, uploads to OpenGL.
// =============================================================================

#include "engine/core/math_types.hpp"

#include <glad/gl.h>

#include <cstdint>
#include <string>
#include <vector>

namespace rf {

class GLTexture {
public:
    GLTexture() = default;
    ~GLTexture();

    // Non-copyable, movable
    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;
    GLTexture(GLTexture&& other) noexcept;
    GLTexture& operator=(GLTexture&& other) noexcept;

    // ----- Loading -----

    /// Load a 2D texture from a file path (uses stb_image).
    bool loadFromFile(const std::string& path);

    /// Load a 2D texture from raw pixel data already in memory.
    /// @param data   Pixel data (RGBA by default).
    /// @param width  Texture width.
    /// @param height Texture height.
    /// @param channels Number of channels (3 = RGB, 4 = RGBA).
    bool loadFromMemory(const std::uint8_t* data, int width, int height, int channels = 4);

    /// Load a cubemap from 6 individual face image paths.
    /// Order: +X, -X, +Y, -Y, +Z, -Z
    bool loadCubemap(const std::string faces[6]);

    /// Load a cubemap from a single equirectangular panorama image.
    bool loadCubemapFromPanorama(const std::string& panoramaPath, int faceSize = 512);

    /// Destroy the GL texture.
    void destroy();

    // ----- Binding -----

    /// Bind to a specific texture unit (0-based).
    void bind(int unit = 0) const;

    /// Unbind the given texture unit.
    static void unbind(int unit = 0);

    // ----- Filtering -----

    enum class Filter {
        Nearest,        ///< Pixel-art style
        Linear,         ///< Bilinear
        NearestMipmap,  ///< Nearest + mipmap
        LinearMipmap,   ///< Trilinear
    };

    void setFilter(Filter filter);

    // ----- Accessors -----

    bool isValid() const { return id_ != 0; }
    GLuint id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool isCubemap() const { return isCubemap_; }

    // ----- Image CPU access -----

    /// Sample a pixel color from the CPU-side image data (if retained).
    /// Returns Color::Blank() if pixel data is not available.
    Color samplePixel(int x, int y) const;

    /// Keep CPU pixel data after upload (needed for colormap sampling).
    void retainPixelData(bool retain) { retainPixels_ = retain; }

private:
    GLuint id_{0};
    int width_{0};
    int height_{0};
    int channels_{0};
    bool isCubemap_{false};

    // Optional CPU-side pixel data (for colormap sampling etc.)
    bool retainPixels_{false};
    std::vector<std::uint8_t> pixelData_;
};

} // namespace rf
