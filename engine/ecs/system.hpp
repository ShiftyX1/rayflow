#pragma once

#include "engine/core/export.hpp"

#include <entt/entt.hpp>

namespace ecs {

class RAYFLOW_CORE_API System {
public:
    virtual ~System() = default;
    virtual void update(entt::registry& registry, float delta_time) = 0;
};

} // namespace ecs
