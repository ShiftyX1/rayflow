#pragma once

// BedWars-specific FPS input system.
// Migrated from engine/ecs/systems/input_system — game-specific because of
// hardcoded WASD+mouse FPS control scheme and camera coupling.

#include "engine/ecs/system.hpp"
#include "engine/ecs/components.hpp"

namespace ecs {

class FpsInputSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    
private:
    void update_player_input(entt::registry& registry);
    void update_camera_look(entt::registry& registry, float delta_time);
};

} // namespace ecs
