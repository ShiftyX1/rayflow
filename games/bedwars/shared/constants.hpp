#pragma once

namespace bedwars {

// Maximum block interaction reach used by both client raycasts and server validation.
inline constexpr float kBlockReachDistance = 5.0f;

// Player eye height above feet (used for camera and reach checks).
inline constexpr float kPlayerEyeHeight = 1.62f;

// Player collision capsule/AABB dimensions (server physics + client collider).
inline constexpr float kPlayerWidth = 0.6f;
inline constexpr float kPlayerHeight = 1.8f;

} // namespace bedwars
