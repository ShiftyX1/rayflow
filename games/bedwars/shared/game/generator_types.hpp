#pragma once

#include "item_types.hpp"
#include <cstdint>

namespace bedwars::game {

// Generator tier determines spawn rate and item type
enum class GeneratorTier : std::uint8_t {
    // Island generators (per-team)
    Iron = 0,      // Fast, iron ingots
    Gold = 1,      // Medium, gold ingots
    
    // Center/mid generators (shared)
    Diamond = 2,   // Slow, diamonds
    Emerald = 3,   // Very slow, emeralds
};

// Get the item type produced by a generator tier
inline ItemType generator_item_type(GeneratorTier tier) {
    switch (tier) {
        case GeneratorTier::Iron:    return ItemType::Iron;
        case GeneratorTier::Gold:    return ItemType::Gold;
        case GeneratorTier::Diamond: return ItemType::Diamond;
        case GeneratorTier::Emerald: return ItemType::Emerald;
        default:                     return ItemType::None;
    }
}

// Default spawn interval in seconds for each tier
inline float default_spawn_interval(GeneratorTier tier) {
    switch (tier) {
        case GeneratorTier::Iron:    return 1.0f;   // 1 iron per second
        case GeneratorTier::Gold:    return 6.0f;   // 1 gold per 6 seconds
        case GeneratorTier::Diamond: return 30.0f;  // 1 diamond per 30 seconds
        case GeneratorTier::Emerald: return 60.0f;  // 1 emerald per minute
        default:                     return 10.0f;
    }
}

// Max items on ground before generator pauses
inline std::uint8_t generator_max_items(GeneratorTier tier) {
    switch (tier) {
        case GeneratorTier::Iron:    return 48;
        case GeneratorTier::Gold:    return 16;
        case GeneratorTier::Diamond: return 8;
        case GeneratorTier::Emerald: return 4;
        default:                     return 16;
    }
}

// Get tier name string
inline const char* generator_tier_name(GeneratorTier tier) {
    switch (tier) {
        case GeneratorTier::Iron:    return "Iron";
        case GeneratorTier::Gold:    return "Gold";
        case GeneratorTier::Diamond: return "Diamond";
        case GeneratorTier::Emerald: return "Emerald";
        default:                     return "Unknown";
    }
}

} // namespace bedwars::game
