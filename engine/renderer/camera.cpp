#include "camera.hpp"

#include <cmath>

namespace rf {

// ============================================================================
// Perspective
// ============================================================================

void Camera::setPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    fovDeg_ = fovDegrees;
    aspect_ = aspectRatio;
    near_ = nearPlane;
    far_ = farPlane;
    projection_ = glm::perspective(glm::radians(fovDeg_), aspect_, near_, far_);
}

void Camera::setAspect(float aspect) {
    aspect_ = aspect;
    projection_ = glm::perspective(glm::radians(fovDeg_), aspect_, near_, far_);
}

// ============================================================================
// Euler angles
// ============================================================================

void Camera::setEulerAngles(float yawDeg, float pitchDeg) {
    float yawRad = glm::radians(yawDeg);
    float pitchRad = glm::radians(pitchDeg);

    Vec3 dir;
    dir.x = std::cos(pitchRad) * std::sin(yawRad);
    dir.y = std::sin(pitchRad);
    dir.z = std::cos(pitchRad) * std::cos(yawRad);

    target_ = position_ + glm::normalize(dir);
    viewDirty_ = true;
}

// ============================================================================
// View matrix (lazy recompute)
// ============================================================================

const Mat4& Camera::viewMatrix() const {
    // Always recompute since position/target may have changed without notification.
    // The cost of a lookAt is negligible compared to rendering.
    view_ = glm::lookAt(position_, target_, up_);
    return view_;
}

} // namespace rf
