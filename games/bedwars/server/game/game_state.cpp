#include "game_state.hpp"

#include <algorithm>

namespace bedwars::server::game {

GameState::GameState() = default;

void GameState::init(const MatchConfig& config) {
    config_ = config;
    
    // Initialize teams
    teamManager_.init_teams(config.teamCount);
    
    // Create beds for each team (positions will be set by map loader)
    beds_.clear();
    beds_.reserve(config.teamCount);
    for (std::size_t i = 0; i < config.teamCount; ++i) {
        Bed bed;
        bed.teamId = teamManager_.teams()[i].id;
        beds_.push_back(bed);
    }
    
    phase_ = MatchPhase::Waiting;
    matchTick_ = 0;
    phaseStartTick_ = 0;
    winner_ = bedwars::game::Teams::None;
}

void GameState::reset() {
    // Reset teams (keep structure, reset state)
    teamManager_.reset();
    
    // Reset beds
    for (auto& bed : beds_) {
        bed.reset();
    }
    
    // Reset players (keep them, reset state)
    for (auto& p : players_) {
        p.reset_stats();
        p.reset_for_respawn();
    }
    
    // Clear generators and items
    generatorManager_.clear();
    
    phase_ = MatchPhase::Waiting;
    matchTick_ = 0;
    phaseStartTick_ = 0;
    winner_ = bedwars::game::Teams::None;
}

PlayerState* GameState::add_player(engine::PlayerId playerId, const std::string& name) {
    // Check if already exists
    if (auto* existing = get_player(playerId)) {
        return existing;
    }
    
    PlayerState player;
    player.playerId = playerId;
    player.name = name;
    player.health = 20;
    player.maxHealth = 20;
    player.isAlive = true;
    player.inventory.give_starting_items();
    
    players_.push_back(std::move(player));
    return &players_.back();
}

void GameState::remove_player(engine::PlayerId playerId) {
    // Remove from team
    teamManager_.remove_player(playerId);
    
    // Remove from players list
    players_.erase(
        std::remove_if(players_.begin(), players_.end(),
            [playerId](const PlayerState& p) { return p.playerId == playerId; }),
        players_.end()
    );
}

PlayerState* GameState::get_player(engine::PlayerId playerId) {
    for (auto& p : players_) {
        if (p.playerId == playerId) return &p;
    }
    return nullptr;
}

const PlayerState* GameState::get_player(engine::PlayerId playerId) const {
    for (const auto& p : players_) {
        if (p.playerId == playerId) return &p;
    }
    return nullptr;
}

bedwars::game::TeamId GameState::assign_player_to_team(engine::PlayerId playerId,
                                                       bedwars::game::TeamId preferred) {
    auto teamId = teamManager_.assign_player(playerId, preferred);
    
    // Update player's team
    if (auto* player = get_player(playerId)) {
        player->teamId = teamId;
    }
    
    return teamId;
}

Bed* GameState::get_team_bed(bedwars::game::TeamId teamId) {
    for (auto& bed : beds_) {
        if (bed.teamId == teamId) return &bed;
    }
    return nullptr;
}

void GameState::update(float deltaTime, std::uint64_t serverTick) {
    matchTick_ = serverTick;
    
    switch (phase_) {
        case MatchPhase::Waiting:
            // Check if enough players to start
            break;
            
        case MatchPhase::Starting:
            // Countdown
            if (serverTick - phaseStartTick_ >= config_.startCountdownTicks) {
                phase_ = MatchPhase::InProgress;
                phaseStartTick_ = serverTick;
            }
            break;
            
        case MatchPhase::InProgress:
            // Update generators
            {
                auto spawned = generatorManager_.update_generators(deltaTime);
                for (const auto& item : spawned) {
                    if (onItemSpawned) onItemSpawned(item);
                }
            }
            
            // Update item despawns
            generatorManager_.update_items(deltaTime);
            
            // Update health regeneration
            update_regeneration(deltaTime, serverTick);
            
            // Update respawns
            update_respawns(serverTick);
            
            // Check win condition
            check_win_condition();
            break;
            
        case MatchPhase::Ending:
            // Wait for end delay
            if (serverTick - phaseStartTick_ >= config_.endDelayTicks) {
                phase_ = MatchPhase::Finished;
            }
            break;
            
        case MatchPhase::Finished:
            // Match complete, waiting for reset
            break;
    }
}

void GameState::start_match() {
    if (phase_ != MatchPhase::Waiting) return;
    
    phase_ = MatchPhase::Starting;
    phaseStartTick_ = matchTick_;
}

void GameState::end_match(bedwars::game::TeamId winner) {
    winner_ = winner;
    phase_ = MatchPhase::Ending;
    phaseStartTick_ = matchTick_;
    
    if (onMatchEnded) onMatchEnded(winner);
}

void GameState::check_win_condition() {
    if (phase_ != MatchPhase::InProgress) return;
    
    // Update team eliminations
    teamManager_.update_eliminations(players_);
    
    // Check for winner
    auto winner = teamManager_.get_winner();
    if (winner != bedwars::game::Teams::None) {
        end_match(winner);
    }
}

std::uint8_t GameState::process_attack(engine::PlayerId attacker, 
                                        engine::PlayerId target,
                                        std::uint64_t tick) {
    auto* attackerPlayer = get_player(attacker);
    auto* targetPlayer = get_player(target);
    
    if (!attackerPlayer || !targetPlayer) return 0;
    if (!attackerPlayer->isAlive || !targetPlayer->isAlive) return 0;
    
    // Check friendly fire
    if (!config_.friendlyFire && attackerPlayer->teamId == targetPlayer->teamId) {
        return 0;
    }
    
    // Check cooldown
    if (!attackerPlayer->can_attack(tick, config_.attackCooldownTicks)) {
        return 0;
    }
    
    // Calculate damage
    std::uint8_t damage = config_.baseMeleeDamage;
    
    // Apply sharpness upgrade
    auto* attackerTeam = teamManager_.get_player_team(attacker);
    if (attackerTeam) {
        damage += attackerTeam->upgrades.sharpness;
    }
    
    // Deal damage
    attackerPlayer->lastAttackTick = tick;
    std::uint8_t dealt = targetPlayer->take_damage(damage, attacker, tick);
    
    if (onHealthChanged) {
        onHealthChanged(target, targetPlayer->health, targetPlayer->maxHealth);
    }
    
    // Check for kill
    if (!targetPlayer->isAlive) {
        process_death(target, tick);
    }
    
    return dealt;
}

void GameState::process_death(engine::PlayerId playerId, std::uint64_t tick) {
    auto* player = get_player(playerId);
    if (!player) return;
    
    // Credit kill
    if (player->lastDamager != 0) {
        if (auto* killer = get_player(player->lastDamager)) {
            killer->kills++;
            
            // Final kill if victim can't respawn
            if (!player->canRespawn) {
                killer->finalKills++;
            }
            
            if (onPlayerKilled) {
                onPlayerKilled(player->lastDamager, playerId);
            }
        }
    }
    
    // Check if team bed is alive
    auto* bed = get_team_bed(player->teamId);
    player->canRespawn = (bed && bed->isAlive);
    player->deathTick = tick;
    
    // Set respawn tick
    if (player->canRespawn) {
        player->respawnTick = tick + config_.respawnDelayTicks;
    }
    
    // Drop items if not keeping inventory
    if (!config_.keepInventoryOnDeath) {
        // TODO: Drop inventory items
        player->inventory.clear();
    }
    
    // Check for team elimination
    teamManager_.update_eliminations(players_);
    auto* team = teamManager_.get_player_team(playerId);
    if (team && team->isEliminated) {
        if (onTeamEliminated) {
            onTeamEliminated(team->id);
        }
    }
}

bool GameState::try_respawn(engine::PlayerId playerId, std::uint64_t tick) {
    auto* player = get_player(playerId);
    if (!player || player->isAlive || !player->canRespawn) {
        return false;
    }
    
    // Check respawn delay
    if (tick < player->respawnTick) {
        return false;
    }
    
    // Get spawn position from team
    auto* team = teamManager_.get_player_team(playerId);
    if (!team) return false;
    
    float x = team->spawn.x;
    float y = team->spawn.y;
    float z = team->spawn.z;
    
    player->respawn(x, y, z);
    
    if (onPlayerRespawned) {
        onPlayerRespawned(playerId, x, y, z);
    }
    
    return true;
}

void GameState::process_bed_destruction(int x, int y, int z, 
                                         engine::PlayerId destroyer,
                                         std::uint64_t tick) {
    auto* bed = get_bed_at(x, y, z);
    if (!bed || !bed->isAlive) return;
    
    // Check if destroyer is on same team (shouldn't be able to destroy own bed)
    auto* destroyerPlayer = get_player(destroyer);
    if (destroyerPlayer && destroyerPlayer->teamId == bed->teamId) {
        return;  // Can't destroy own bed
    }
    
    bed->destroy(destroyer, tick);
    
    // Update team
    auto* team = teamManager_.get_team(bed->teamId);
    if (team) {
        team->destroy_bed();
    }
    
    // Disable respawn for all team members
    for (auto& p : players_) {
        if (p.teamId == bed->teamId) {
            p.canRespawn = false;
        }
    }
    
    // Credit bed destroy
    if (destroyerPlayer) {
        destroyerPlayer->bedsDestroyed++;
    }
    
    if (onBedDestroyed) {
        onBedDestroyed(bed->teamId, destroyer);
    }
}

Bed* GameState::get_bed_at(int x, int y, int z) {
    for (auto& bed : beds_) {
        if (bed.contains_position(x, y, z)) {
            return &bed;
        }
    }
    return nullptr;
}

void GameState::process_item_pickup(engine::PlayerId playerId) {
    auto* player = get_player(playerId);
    if (!player || !player->isAlive) return;
    
    // TODO: Get player position from physics state
    float px = 0, py = 0, pz = 0;  // Need to get from DedicatedServer
    
    auto nearby = generatorManager_.find_items_near(px, py, pz, config_.itemPickupRadius);
    
    for (auto itemId : nearby) {
        auto* item = generatorManager_.get_item(itemId);
        if (!item || !item->can_pickup()) continue;
        
        // Add to inventory
        if (bedwars::game::is_resource(item->itemType)) {
            player->inventory.add_resource(item->itemType, item->count);
        } else {
            if (player->inventory.add_to_hotbar(item->itemType, item->count) < 0) {
                continue;  // Inventory full
            }
        }
        
        if (onItemPickedUp) {
            onItemPickedUp(itemId);
        }
        
        generatorManager_.remove_item(itemId);
    }
}

void GameState::update_regeneration(float /*deltaTime*/, std::uint64_t tick) {
    for (auto& p : players_) {
        if (p.should_regen(tick, config_.regenDelayTicks)) {
            p.heal(1);
            if (onHealthChanged) {
                onHealthChanged(p.playerId, p.health, p.maxHealth);
            }
        }
    }
}

void GameState::update_respawns(std::uint64_t tick) {
    for (auto& p : players_) {
        if (!p.isAlive && p.canRespawn && tick >= p.respawnTick) {
            try_respawn(p.playerId, tick);
        }
    }
}

} // namespace bedwars::server::game
