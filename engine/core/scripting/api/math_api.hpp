#pragma once

// =============================================================================
// Math API Module — math utility functions + vec2/vec3 types for Lua scripts
// Provides: vec2(x,y), vec3(x,y,z) usertypes, lerp, clamp, distance, etc.
// =============================================================================

namespace sol { class state; }

namespace engine::scripting::api {

/// Register math utility API in Lua:
///   vec2 usertype (x, y, +, -, *, length, normalize, distance)
///   vec3 usertype (x, y, z, +, -, *, length, normalize, distance)
///   math.lerp(a, b, t)
///   math.clamp(val, min, max)
///   math.distance(vec_a, vec_b)  — works with both vec2 and vec3
///   math.remap(val, in_min, in_max, out_min, out_max)
void register_math_api(sol::state& lua);

} // namespace engine::scripting::api
