#pragma once

// =============================================================================
// math_types.hpp — Engine math type aliases (backed by glm)
// =============================================================================
//
// This header defines the canonical math types for the engine.
// All engine/game code should use these types instead of raw glm or raylib types.
//
// During the migration period, raylib types (Vector2, Vector3, etc.) are being
// replaced with these rf:: types throughout the codebase.
//

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace rf {

// ---- Vector / Matrix aliases ------------------------------------------------

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using IVec2 = glm::ivec2;
using IVec3 = glm::ivec3;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat;

// ---- Color ------------------------------------------------------------------

struct Color {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};

    constexpr Color() = default;
    constexpr Color(std::uint8_t r_, std::uint8_t g_, std::uint8_t b_, std::uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    constexpr bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    constexpr bool operator!=(const Color& o) const { return !(*this == o); }

    /// Normalise to float [0,1] vec4 (useful for OpenGL uniforms)
    glm::vec4 normalized() const {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    // Named constants
    static constexpr Color White()     { return {255, 255, 255, 255}; }
    static constexpr Color Black()     { return {  0,   0,   0, 255}; }
    static constexpr Color Red()       { return {255,   0,   0, 255}; }
    static constexpr Color Green()     { return {  0, 255,   0, 255}; }
    static constexpr Color Blue()      { return {  0,   0, 255, 255}; }
    static constexpr Color Yellow()    { return {255, 255,   0, 255}; }
    static constexpr Color Magenta()   { return {255,   0, 255, 255}; }
    static constexpr Color Cyan()      { return {  0, 255, 255, 255}; }
    static constexpr Color Gray()      { return {128, 128, 128, 255}; }
    static constexpr Color DarkGray()  { return { 80,  80,  80, 255}; }
    static constexpr Color LightGray() { return {200, 200, 200, 255}; }
    static constexpr Color Orange()    { return {255, 161,   0, 255}; }
    static constexpr Color Pink()      { return {255, 109, 194, 255}; }
    static constexpr Color Purple()    { return {200, 122, 255, 255}; }
    static constexpr Color Blank()     { return {  0,   0,   0,   0}; }
};

// ---- Rectangle (axis-aligned) -----------------------------------------------

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};

    constexpr Rect() = default;
    constexpr Rect(float x_, float y_, float w_, float h_)
        : x(x_), y(y_), w(w_), h(h_) {}

    constexpr bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }

    constexpr bool contains(Vec2 p) const {
        return contains(p.x, p.y);
    }
};

} // namespace rf
