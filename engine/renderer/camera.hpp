#pragma once

// =============================================================================
// Camera — 3D camera system
//
// Phase 2: Replaces raylib Camera3D / UpdateCamera / GetCameraMatrix etc.
// Computes view and projection matrices using glm.
// =============================================================================

#include "engine/core/math_types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rf {

class Camera {
public:
    Camera() = default;

    /// Set perspective projection parameters.
    void setPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);

    /// Update aspect ratio (e.g. on window resize).
    void setAspect(float aspect);

    // ----- Position & Orientation -----

    void setPosition(const Vec3& pos) { position_ = pos; }
    void setTarget(const Vec3& target) { target_ = target; }
    void setUp(const Vec3& up) { up_ = up; }

    /// Set camera from Euler angles (yaw, pitch in degrees).
    /// Computes the look-at target from the current position.
    void setEulerAngles(float yawDeg, float pitchDeg);

    const Vec3& position() const { return position_; }
    const Vec3& target() const { return target_; }
    const Vec3& up() const { return up_; }

    /// Forward direction (normalized).
    Vec3 forward() const { return glm::normalize(target_ - position_); }

    /// Right direction (normalized).
    Vec3 right() const { return glm::normalize(glm::cross(forward(), up_)); }

    // ----- Matrices -----

    /// Recompute view matrix from current position/target/up.
    const Mat4& viewMatrix() const;

    /// Projection matrix (set via setPerspective).
    const Mat4& projectionMatrix() const { return projection_; }

    /// Combined view-projection matrix.
    Mat4 viewProjectionMatrix() const { return projection_ * viewMatrix(); }

    // ----- Parameters -----

    float fov() const { return fovDeg_; }
    float nearPlane() const { return near_; }
    float farPlane() const { return far_; }
    float aspect() const { return aspect_; }

private:
    Vec3 position_{0.0f, 0.0f, 0.0f};
    Vec3 target_{0.0f, 0.0f, -1.0f};
    Vec3 up_{0.0f, 1.0f, 0.0f};

    float fovDeg_{60.0f};
    float aspect_{16.0f / 9.0f};
    float near_{0.1f};
    float far_{1000.0f};

    Mat4 projection_{1.0f};
    mutable Mat4 view_{1.0f};
    mutable bool viewDirty_{true};
};

} // namespace rf
