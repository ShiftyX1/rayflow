#pragma once

// BedWars-specific ECS components.
// Migrated from engine/ecs/components.hpp — game-specific types
// (ToolHolder with Minecraft-style tool enums, BlockBreaker state).

#include "engine/core/math_types.hpp"

namespace ecs {

struct ToolHolder {
    enum class ToolType {
        None,
        Pickaxe,
        Axe,
        Shovel,
        Sword
    };
    
    enum class ToolLevel {
        Hand,
        Wood,
        Stone,
        Iron,
        Diamond
    };
    
    ToolType tool_type{ToolType::None};
    ToolLevel tool_level{ToolLevel::Hand};
    
    float get_mining_speed() const;
    int get_harvest_level() const;
};

struct BlockBreaker {
    bool is_breaking{false};
    float break_progress{0.0f};
    int target_block_x{0};
    int target_block_y{0};
    int target_block_z{0};
    bool has_target{false};
};

} // namespace ecs
