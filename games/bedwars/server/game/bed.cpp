#include "bed.hpp"

namespace bedwars::server::game {

void Bed::get_foot_position(int& outX, int& outY, int& outZ) const {
    outX = x;
    outY = y;
    outZ = z;
    
    // Offset based on direction
    switch (direction) {
        case 0: outX -= 1; break;  // +X direction, foot at x-1
        case 1: outX += 1; break;  // -X direction, foot at x+1
        case 2: outZ -= 1; break;  // +Z direction, foot at z-1
        case 3: outZ += 1; break;  // -Z direction, foot at z+1
    }
}

bool Bed::contains_position(int px, int py, int pz) const {
    // Check head position
    if (px == x && py == y && pz == z) {
        return true;
    }
    
    // Check foot position
    int fx, fy, fz;
    get_foot_position(fx, fy, fz);
    return (px == fx && py == fy && pz == fz);
}

void Bed::destroy(engine::PlayerId destroyer, std::uint64_t tick) {
    if (!isAlive) return;
    
    isAlive = false;
    destroyedBy = destroyer;
    destroyedAtTick = tick;
}

void Bed::reset() {
    isAlive = true;
    destroyedBy = 0;
    destroyedAtTick = 0;
}

} // namespace bedwars::server::game
