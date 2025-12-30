#pragma once

#include "../../shared/game/team_types.hpp"
#include "../../shared/protocol/messages.hpp"
#include "inventory.hpp"

#include <cstdint>
#include <string>

namespace server::game {

// Extended player state for BedWars gameplay
// This augments the basic physics state in DedicatedServer::ClientState
struct PlayerState {
    // Identity
    shared::proto::PlayerId playerId{0};
    std::string name;
    shared::game::TeamId teamId{shared::game::Teams::None};
    
    // Health system
    std::uint8_t health{20};
    std::uint8_t maxHealth{20};
    bool isAlive{true};
    
    // Combat
    std::uint64_t lastDamageTick{0};     // For regen delay
    std::uint64_t lastAttackTick{0};     // For attack cooldown
    shared::proto::PlayerId lastDamager{0}; // For kill credit
    
    // Respawn
    bool canRespawn{true};  // False when bed is destroyed
    std::uint64_t deathTick{0};
    std::uint64_t respawnTick{0};
    
    // Inventory
    Inventory inventory;
    
    // Stats (for scoreboard)
    std::uint32_t kills{0};
    std::uint32_t deaths{0};
    std::uint32_t bedsDestroyed{0};
    std::uint32_t finalKills{0};  // Kills when victim can't respawn
    
    // === Methods ===
    
    // Take damage, returns actual damage dealt
    std::uint8_t take_damage(std::uint8_t amount, shared::proto::PlayerId attacker, std::uint64_t tick);
    
    // Heal player
    void heal(std::uint8_t amount);
    
    // Kill player (called when health reaches 0)
    void die(std::uint64_t tick);
    
    // Respawn player at position
    void respawn(float x, float y, float z);
    
    // Check if player can attack (cooldown)
    bool can_attack(std::uint64_t currentTick, std::uint32_t cooldownTicks) const;
    
    // Check if player should regenerate health
    bool should_regen(std::uint64_t currentTick, std::uint32_t regenDelayTicks) const;
    
    // Reset stats for new match
    void reset_stats();
    
    // Reset for respawn (keep stats, reset health/inventory)
    void reset_for_respawn();
};

} // namespace server::game
