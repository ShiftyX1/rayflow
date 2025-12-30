#include "player_state.hpp"

#include <algorithm>

namespace server::game {

std::uint8_t PlayerState::take_damage(std::uint8_t amount, shared::proto::PlayerId attacker, std::uint64_t tick) {
    if (!isAlive || amount == 0) return 0;
    
    // Apply armor reduction (simplified: 4% per armor tier point)
    std::uint8_t armorTier = inventory.get_armor_tier();
    float reduction = static_cast<float>(armorTier) * 0.04f;
    float reducedDamage = static_cast<float>(amount) * (1.0f - reduction);
    std::uint8_t finalDamage = std::max(1, static_cast<int>(reducedDamage));
    
    // Apply damage
    if (finalDamage >= health) {
        finalDamage = health;
        health = 0;
        die(tick);
    } else {
        health -= finalDamage;
    }
    
    // Track for kill credit and regen delay
    lastDamageTick = tick;
    if (attacker != 0) {
        lastDamager = attacker;
    }
    
    return finalDamage;
}

void PlayerState::heal(std::uint8_t amount) {
    if (!isAlive) return;
    
    health = std::min(static_cast<std::uint8_t>(health + amount), maxHealth);
}

void PlayerState::die(std::uint64_t tick) {
    isAlive = false;
    deathTick = tick;
    deaths++;
    
    // Drop resources on death (inventory clear happens on respawn)
}

void PlayerState::respawn(float /*x*/, float /*y*/, float /*z*/) {
    isAlive = true;
    health = maxHealth;
    lastDamageTick = 0;
    lastAttackTick = 0;
    lastDamager = 0;
    
    // Reset inventory but keep permanent upgrades
    inventory.give_starting_items();
}

bool PlayerState::can_attack(std::uint64_t currentTick, std::uint32_t cooldownTicks) const {
    return (currentTick - lastAttackTick) >= cooldownTicks;
}

bool PlayerState::should_regen(std::uint64_t currentTick, std::uint32_t regenDelayTicks) const {
    if (!isAlive || health >= maxHealth) return false;
    return (currentTick - lastDamageTick) >= regenDelayTicks;
}

void PlayerState::reset_stats() {
    kills = 0;
    deaths = 0;
    bedsDestroyed = 0;
    finalKills = 0;
}

void PlayerState::reset_for_respawn() {
    health = maxHealth;
    isAlive = true;
    lastDamageTick = 0;
    lastAttackTick = 0;
    lastDamager = 0;
    inventory.give_starting_items();
}

} // namespace server::game
