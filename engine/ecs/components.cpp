#include "components.hpp"

namespace ecs {

float ToolHolder::get_mining_speed() const {
    float base_speed = 1.0f;
    
    switch (tool_level) {
        case ToolLevel::Hand:    base_speed = 1.0f; break;
        case ToolLevel::Wood:    base_speed = 2.0f; break;
        case ToolLevel::Stone:   base_speed = 4.0f; break;
        case ToolLevel::Iron:    base_speed = 6.0f; break;
        case ToolLevel::Diamond: base_speed = 8.0f; break;
    }
    
    return base_speed;
}

int ToolHolder::get_harvest_level() const {
    switch (tool_level) {
        case ToolLevel::Hand:    return 0;
        case ToolLevel::Wood:    return 1;
        case ToolLevel::Stone:   return 2;
        case ToolLevel::Iron:    return 3;
        case ToolLevel::Diamond: return 4;
    }
    return 0;
}

} // namespace ecs
