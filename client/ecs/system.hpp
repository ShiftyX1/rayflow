#pragma once

#include <entt/entt.hpp>

namespace ecs {

class System {
public:
    virtual ~System() = default;
    virtual void update(entt::registry& registry, float delta_time) = 0;
};

} // namespace ecs
