#pragma once

#include "../system.hpp"
#include "../components.hpp"

namespace ecs {

class InputSystem : public System {
public:
    void update(entt::registry& registry, float delta_time) override;
    
private:
    void update_player_input(entt::registry& registry);
    void update_camera_look(entt::registry& registry, float delta_time);
};

} // namespace ecs
