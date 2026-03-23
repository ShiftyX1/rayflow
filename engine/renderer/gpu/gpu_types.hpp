#pragma once

// =============================================================================
// gpu_types.hpp — Backend-agnostic GPU type definitions
//
// All enums and structs used by the abstract GPU interfaces.
// No OpenGL / DirectX headers included here.
// =============================================================================

#include <cstdint>

namespace rf {

/// Render backend identifier.
enum class Backend {
    OpenGL,
    DirectX11,
};

/// Primitive topology for draw calls.
enum class PrimitiveType {
    Triangles,
    Lines,
    LineStrip,
    Points,
};

/// Texture filtering mode.
enum class TextureFilter {
    Nearest,        ///< Pixel-art style
    Linear,         ///< Bilinear
    NearestMipmap,  ///< Nearest + nearest mip
    LinearMipmap,   ///< Trilinear
};

/// Texture wrapping mode.
enum class TextureWrap {
    Repeat,
    ClampToEdge,
    MirroredRepeat,
};

/// Internal texture / framebuffer formats.
enum class TextureFormat {
    RGBA8,
    RGBA16F,
    RGBA32F,
    RGB8,
    R8,
    RG8,
};

/// Depth buffer formats.
enum class DepthFormat {
    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8,
};

/// Framebuffer attachment descriptor (backend-agnostic).
struct AttachmentDesc {
    TextureFormat format = TextureFormat::RGBA8;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap   wrap   = TextureWrap::ClampToEdge;
};

/// Vertex attribute semantic indices (shared across all backends).
/// Must match shader layout locations.
enum Attrib : std::uint32_t {
    ATTRIB_POSITION   = 0,  // vec3
    ATTRIB_TEXCOORD   = 1,  // vec2
    ATTRIB_TEXCOORD2  = 2,  // vec2 (foliage mask, AO)
    ATTRIB_NORMAL     = 3,  // vec3
    ATTRIB_COLOR      = 4,  // vec4 (RGBA ubyte normalized)
};

/// Blend mode presets.
enum class BlendMode {
    None,           ///< No blending (opaque)
    Alpha,          ///< Standard alpha blending
    Additive,       ///< Additive blending
    Premultiplied,  ///< Premultiplied alpha
};

/// Face culling mode.
enum class CullMode {
    None,
    Back,
    Front,
};

/// Depth comparison function.
enum class DepthFunc {
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    Always,
    Never,
};

} // namespace rf
