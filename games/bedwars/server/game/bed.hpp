#pragma once

#include "../../shared/game/team_types.hpp"
#include "../../shared/protocol/messages.hpp"

#include <cstdint>

namespace bedwars::server::game {

// Bed entity state
struct Bed {
    // Position (head block position)
    int x{0};
    int y{0};
    int z{0};
    
    // Direction (which way the bed faces)
    // 0=+X, 1=-X, 2=+Z, 3=-Z
    std::uint8_t direction{0};
    
    // Owner team
    bedwars::game::TeamId teamId{bedwars::game::Teams::None};
    
    // State
    bool isAlive{true};
    
    // Destroyer info (when broken)
    engine::PlayerId destroyedBy{0};
    std::uint64_t destroyedAtTick{0};
    
    // === Methods ===
    
    // Get foot block position based on direction
    void get_foot_position(int& outX, int& outY, int& outZ) const;
    
    // Check if position is part of this bed
    bool contains_position(int px, int py, int pz) const;
    
    // Destroy the bed
    void destroy(engine::PlayerId destroyer, std::uint64_t tick);
    
    // Reset for new match
    void reset();
};

} // namespace bedwars::server::game
