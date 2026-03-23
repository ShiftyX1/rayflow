#pragma once

// =============================================================================
// ITexture — Abstract texture interface
//
// Backend-agnostic. Implemented by GLTexture, DX11Texture, etc.
// =============================================================================

#include "engine/core/export.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gpu/gpu_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rf {

class RAYFLOW_CLIENT_API ITexture {
public:
    virtual ~ITexture() = default;

    // Non-copyable
    ITexture(const ITexture&) = delete;
    ITexture& operator=(const ITexture&) = delete;

    // ----- Loading -----

    /// Load a 2D texture from a file path.
    virtual bool loadFromFile(const std::string& path) = 0;

    /// Load a 2D texture from raw pixel data.
    virtual bool loadFromMemory(const std::uint8_t* data, int width, int height, int channels = 4) = 0;

    /// Load a cubemap from 6 individual face image paths.
    /// Order: +X, -X, +Y, -Y, +Z, -Z
    virtual bool loadCubemap(const std::string faces[6]) = 0;

    /// Load a cubemap from a single equirectangular panorama.
    virtual bool loadCubemapFromPanorama(const std::string& panoramaPath, int faceSize = 512) = 0;

    /// Release GPU resources.
    virtual void destroy() = 0;

    // ----- Binding -----

    /// Bind to a texture unit (0-based).
    virtual void bind(int unit = 0) const = 0;

    /// Unbind the given texture unit.
    virtual void unbind(int unit = 0) const = 0;

    // ----- Filtering -----

    virtual void setFilter(TextureFilter filter) = 0;

    // ----- Accessors -----

    virtual bool isValid() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool isCubemap() const = 0;

    // ----- CPU-side access -----

    /// Sample a pixel from retained CPU data.
    virtual Color samplePixel(int x, int y) const = 0;

    /// Keep CPU pixel data after upload.
    virtual void retainPixelData(bool retain) = 0;

    /// Get backend-specific native texture handle (GLuint, ID3D11ShaderResourceView*, etc.).
    /// Callers must cast to the appropriate type.
    virtual std::uintptr_t nativeHandle() const = 0;

protected:
    ITexture() = default;
    ITexture(ITexture&&) noexcept = default;
    ITexture& operator=(ITexture&&) noexcept = default;
};

} // namespace rf
