#pragma once

#include "../../shared/game/team_types.hpp"
#include "../../shared/protocol/messages.hpp"

#include <cstdint>

namespace server::game {

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
    shared::game::TeamId teamId{shared::game::Teams::None};
    
    // State
    bool isAlive{true};
    
    // Destroyer info (when broken)
    shared::proto::PlayerId destroyedBy{0};
    std::uint64_t destroyedAtTick{0};
    
    // === Methods ===
    
    // Get foot block position based on direction
    void get_foot_position(int& outX, int& outY, int& outZ) const;
    
    // Check if position is part of this bed
    bool contains_position(int px, int py, int pz) const;
    
    // Destroy the bed
    void destroy(shared::proto::PlayerId destroyer, std::uint64_t tick);
    
    // Reset for new match
    void reset();
};

} // namespace server::game
