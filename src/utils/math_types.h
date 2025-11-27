#ifndef MATH_TYPES_H
#define MATH_TYPES_H

#include <raylib.h>
#include <math.h>
#include <string.h>

// Use Raylib's Vector3 and Vector2
typedef Vector3 Vec3;
typedef Vector2 Vec2;

// Transform component
typedef struct {
    Vector3 position;
    Vector3 rotation;  // Euler angles
    Vector3 scale;
} EntityTransform;

// Math utility functions
static inline Vec3 vec3_new(float x, float y, float z) {
    return (Vec3){x, y, z};
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vec3_scale(Vec3 v, float s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

static inline float vec3_length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    if (len > 0.0f) {
        return vec3_scale(v, 1.0f / len);
    }
    return v;
}

static inline EntityTransform transform_identity(void) {
    return (EntityTransform){
        .position = {0.0f, 0.0f, 0.0f},
        .rotation = {0.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    };
}

#endif // MATH_TYPES_H
