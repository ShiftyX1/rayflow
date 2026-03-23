#include "gl_texture.hpp"

#include "engine/core/logging.hpp"
#include "engine/vfs/vfs.hpp"

#include <stb_image.h>

#include <cstring>

namespace rf {

// ============================================================================
// Move semantics
// ============================================================================

GLTexture::~GLTexture() {
    destroy();
}

GLTexture::GLTexture(GLTexture&& other) noexcept
    : id_(other.id_)
    , width_(other.width_)
    , height_(other.height_)
    , channels_(other.channels_)
    , isCubemap_(other.isCubemap_)
    , retainPixels_(other.retainPixels_)
    , pixelData_(std::move(other.pixelData_))
{
    other.id_ = 0;
    other.width_ = 0;
    other.height_ = 0;
}

GLTexture& GLTexture::operator=(GLTexture&& other) noexcept {
    if (this != &other) {
        destroy();
        id_ = other.id_;
        width_ = other.width_;
        height_ = other.height_;
        channels_ = other.channels_;
        isCubemap_ = other.isCubemap_;
        retainPixels_ = other.retainPixels_;
        pixelData_ = std::move(other.pixelData_);
        other.id_ = 0;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

// ============================================================================
// Loading from file
// ============================================================================

bool GLTexture::loadFromFile(const std::string& path) {
    destroy();

    // Try VFS first (PAK mode), then fall back to direct file
    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = nullptr;

#if RAYFLOW_USE_PAK
    auto fileData = engine::vfs::read_file(path);
    if (fileData) {
        pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(fileData->data()),
            static_cast<int>(fileData->size()),
            &w, &h, &ch, 4);  // force RGBA
    }
#endif

    if (!pixels) {
        // Direct file load (debug / loose files mode)
        stbi_set_flip_vertically_on_load(0);
        pixels = stbi_load(path.c_str(), &w, &h, &ch, 4);
    }

    if (!pixels) {
        TraceLog(LOG_WARNING, "[GLTexture] Failed to load: %s", path.c_str());
        return false;
    }

    bool ok = loadFromMemory(pixels, w, h, 4);
    stbi_image_free(pixels);

    if (ok) {
        TraceLog(LOG_INFO, "[GLTexture] Loaded %s (%dx%d)", path.c_str(), w, h);
    }
    return ok;
}

// ============================================================================
// Loading from memory
// ============================================================================

bool GLTexture::loadFromMemory(const std::uint8_t* data, int width, int height, int channels) {
    destroy();

    if (!data || width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;
    channels_ = channels;

    GLenum internalFormat = GL_RGBA8;
    GLenum format = GL_RGBA;
    if (channels == 3) {
        internalFormat = GL_RGB8;
        format = GL_RGB;
    } else if (channels == 1) {
        internalFormat = GL_R8;
        format = GL_RED;
    }

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);

    // Set default filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Optionally retain pixel data for CPU-side sampling
    if (retainPixels_) {
        std::size_t size = static_cast<std::size_t>(width) * height * channels;
        pixelData_.assign(data, data + size);
    }

    return true;
}

// ============================================================================
// Cubemap loading
// ============================================================================

bool GLTexture::loadCubemap(const std::string faces[6]) {
    destroy();

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id_);

    for (int i = 0; i < 6; ++i) {
        int w = 0, h = 0, ch = 0;
        stbi_uc* pixels = stbi_load(faces[i].c_str(), &w, &h, &ch, 4);
        if (!pixels) {
            TraceLog(LOG_ERROR, "[GLTexture] Cubemap face %d failed: %s", i, faces[i].c_str());
            destroy();
            return false;
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8,
                     w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        if (i == 0) { width_ = w; height_ = h; }
        stbi_image_free(pixels);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    isCubemap_ = true;
    channels_ = 4;

    TraceLog(LOG_INFO, "[GLTexture] Cubemap loaded (%dx%d per face)", width_, height_);
    return true;
}

bool GLTexture::loadCubemapFromPanorama(const std::string& panoramaPath, int faceSize) {
    // Load the panorama image
    int w = 0, h = 0, ch = 0;
    stbi_set_flip_vertically_on_load(0);
    stbi_uc* panoPixels = stbi_load(panoramaPath.c_str(), &w, &h, &ch, 4);
    if (!panoPixels) {
        TraceLog(LOG_ERROR, "[GLTexture] Failed to load panorama: %s", panoramaPath.c_str());
        return false;
    }

    // For panorama conversion, we use a simple equirectangular → cubemap approach.
    // Each face is faceSize x faceSize.
    destroy();

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id_);

    std::vector<std::uint8_t> facePixels(faceSize * faceSize * 4);

    // Direction vectors for cubemap face sampling
    // Face order: +X, -X, +Y, -Y, +Z, -Z
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < faceSize; ++y) {
            for (int x = 0; x < faceSize; ++x) {
                // Map pixel to [-1, 1] face coordinates
                float fx = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
                float fy = (2.0f * (y + 0.5f) / faceSize) - 1.0f;

                float dx = 0, dy = 0, dz = 0;
                switch (face) {
                    case 0: dx = 1;  dy = -fy; dz = -fx; break; // +X
                    case 1: dx = -1; dy = -fy; dz = fx;  break; // -X
                    case 2: dx = fx; dy = 1;   dz = fy;  break; // +Y
                    case 3: dx = fx; dy = -1;  dz = -fy; break; // -Y
                    case 4: dx = fx; dy = -fy; dz = 1;   break; // +Z
                    case 5: dx = -fx; dy = -fy; dz = -1; break; // -Z
                }

                float len = std::sqrt(dx*dx + dy*dy + dz*dz);
                dx /= len; dy /= len; dz /= len;

                // Convert direction to equirectangular UV
                float phi = std::atan2(dz, dx);         // -PI..PI
                float theta = std::asin(dy);             // -PI/2..PI/2
                float u = 0.5f + phi / (2.0f * 3.14159265f);
                float v = 0.5f + theta / 3.14159265f;

                // Sample panorama
                int px = static_cast<int>(u * w) % w;
                int py = static_cast<int>(v * h) % h;
                if (px < 0) px += w;
                if (py < 0) py += h;

                int srcIdx = (py * w + px) * 4;
                int dstIdx = (y * faceSize + x) * 4;
                facePixels[dstIdx + 0] = panoPixels[srcIdx + 0];
                facePixels[dstIdx + 1] = panoPixels[srcIdx + 1];
                facePixels[dstIdx + 2] = panoPixels[srcIdx + 2];
                facePixels[dstIdx + 3] = panoPixels[srcIdx + 3];
            }
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
                     faceSize, faceSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, facePixels.data());
    }

    stbi_image_free(panoPixels);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    width_ = faceSize;
    height_ = faceSize;
    channels_ = 4;
    isCubemap_ = true;

    TraceLog(LOG_INFO, "[GLTexture] Cubemap from panorama %s (%dx%d per face)", panoramaPath.c_str(), faceSize, faceSize);
    return true;
}

// ============================================================================
// Destroy
// ============================================================================

void GLTexture::destroy() {
    if (id_) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
    width_ = 0;
    height_ = 0;
    channels_ = 0;
    isCubemap_ = false;
    pixelData_.clear();
}

// ============================================================================
// Binding
// ============================================================================

void GLTexture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    if (isCubemap_) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
    } else {
        glBindTexture(GL_TEXTURE_2D, id_);
    }
}

void GLTexture::unbind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ============================================================================
// Filtering
// ============================================================================

void GLTexture::setFilter(TextureFilter filter) {
    if (!id_) return;

    GLenum target = isCubemap_ ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    glBindTexture(target, id_);

    switch (filter) {
        case TextureFilter::Nearest:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case TextureFilter::Linear:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case TextureFilter::NearestMipmap:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case TextureFilter::LinearMipmap:
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }

    glBindTexture(target, 0);
}

// ============================================================================
// CPU-side pixel sampling
// ============================================================================

Color GLTexture::samplePixel(int x, int y) const {
    if (pixelData_.empty() || x < 0 || y < 0 || x >= width_ || y >= height_) {
        return Color::Blank();
    }
    std::size_t idx = (static_cast<std::size_t>(y) * width_ + x) * channels_;
    if (idx + channels_ > pixelData_.size()) return Color::Blank();

    std::uint8_t r = pixelData_[idx];
    std::uint8_t g = (channels_ >= 2) ? pixelData_[idx + 1] : r;
    std::uint8_t b = (channels_ >= 3) ? pixelData_[idx + 2] : r;
    std::uint8_t a = (channels_ >= 4) ? pixelData_[idx + 3] : 255;
    return Color(r, g, b, a);
}

} // namespace rf
