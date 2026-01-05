#pragma once

#include <cstdint>
#include <string>

namespace bedwars::game {

// Team identifier (0 = no team / spectator)
using TeamId = std::uint8_t;

// Predefined team IDs for BedWars
namespace Teams {
    constexpr TeamId None     = 0;
    constexpr TeamId Red      = 1;
    constexpr TeamId Blue     = 2;
    constexpr TeamId Green    = 3;
    constexpr TeamId Yellow   = 4;
    constexpr TeamId Aqua     = 5;
    constexpr TeamId White    = 6;
    constexpr TeamId Pink     = 7;
    constexpr TeamId Gray     = 8;
    
    constexpr TeamId MaxTeams = 8;
}

// Team color (RGB)
struct TeamColor {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    
    static constexpr TeamColor from_team_id(TeamId id) {
        switch (id) {
            case Teams::Red:    return {255, 85, 85};
            case Teams::Blue:   return {85, 85, 255};
            case Teams::Green:  return {85, 255, 85};
            case Teams::Yellow: return {255, 255, 85};
            case Teams::Aqua:   return {85, 255, 255};
            case Teams::White:  return {255, 255, 255};
            case Teams::Pink:   return {255, 85, 255};
            case Teams::Gray:   return {170, 170, 170};
            default:            return {255, 255, 255};
        }
    }
};

// Get team name string
inline const char* team_name(TeamId id) {
    switch (id) {
        case Teams::Red:    return "Red";
        case Teams::Blue:   return "Blue";
        case Teams::Green:  return "Green";
        case Teams::Yellow: return "Yellow";
        case Teams::Aqua:   return "Aqua";
        case Teams::White:  return "White";
        case Teams::Pink:   return "Pink";
        case Teams::Gray:   return "Gray";
        default:            return "None";
    }
}

} // namespace bedwars::game
