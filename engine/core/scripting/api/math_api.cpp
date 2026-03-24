#include "math_api.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <cmath>
#include <algorithm>

namespace engine::scripting::api {

// Lightweight vector types exposed to Lua
struct Vec2 {
    float x{0.0f}, y{0.0f};

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }

    float length() const { return std::sqrt(x * x + y * y); }
    Vec2 normalized() const {
        float len = length();
        return len > 0.0f ? Vec2{x / len, y / len} : Vec2{0, 0};
    }
    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float distance_to(const Vec2& o) const { return (*this - o).length(); }

    std::string to_string() const {
        return "vec2(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }
};

struct Vec3 {
    float x{0.0f}, y{0.0f}, z{0.0f};

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        float len = length();
        return len > 0.0f ? Vec3{x / len, y / len, z / len} : Vec3{0, 0, 0};
    }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float distance_to(const Vec3& o) const { return (*this - o).length(); }

    std::string to_string() const {
        return "vec3(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
    }
};

void register_math_api(sol::state& lua) {
    // ---- vec2 usertype ----
    lua.new_usertype<Vec2>("vec2",
        sol::call_constructor, sol::constructors<Vec2(), Vec2(float, float)>(),
        "x", &Vec2::x,
        "y", &Vec2::y,
        sol::meta_function::addition, &Vec2::operator+,
        sol::meta_function::subtraction, &Vec2::operator-,
        sol::meta_function::multiplication, &Vec2::operator*,
        sol::meta_function::to_string, &Vec2::to_string,
        "length", &Vec2::length,
        "normalized", &Vec2::normalized,
        "dot", &Vec2::dot,
        "distance_to", &Vec2::distance_to
    );

    // ---- vec3 usertype ----
    lua.new_usertype<Vec3>("vec3",
        sol::call_constructor, sol::constructors<Vec3(), Vec3(float, float, float)>(),
        "x", &Vec3::x,
        "y", &Vec3::y,
        "z", &Vec3::z,
        sol::meta_function::addition, &Vec3::operator+,
        sol::meta_function::subtraction, &Vec3::operator-,
        sol::meta_function::multiplication, &Vec3::operator*,
        sol::meta_function::to_string, &Vec3::to_string,
        "length", &Vec3::length,
        "normalized", &Vec3::normalized,
        "dot", &Vec3::dot,
        "cross", &Vec3::cross,
        "distance_to", &Vec3::distance_to
    );

    // ---- Math utility extensions ----
    // Add to the existing math table (opened by sol2)
    sol::table math = lua["math"];
    if (!math.valid()) {
        math = lua.create_named_table("math");
    }

    math["lerp"] = [](float a, float b, float t) -> float {
        return a + (b - a) * t;
    };

    math["clamp"] = [](float val, float lo, float hi) -> float {
        return std::clamp(val, lo, hi);
    };

    math["remap"] = [](float val, float inMin, float inMax, float outMin, float outMax) -> float {
        if (inMax == inMin) return outMin;
        float t = (val - inMin) / (inMax - inMin);
        return outMin + (outMax - outMin) * t;
    };
}

} // namespace engine::scripting::api
