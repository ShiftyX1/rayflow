#pragma once

#include "player_state.hpp"
#include "team.hpp"
#include "generator.hpp"
#include "bed.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bedwars::server::game {

// Match phase
enum class MatchPhase : std::uint8_t {
    Waiting,        // Waiting for players
    Starting,       // Countdown before start
    InProgress,     // Match running
    Ending,         // Match ending (winner determined)
    Finished        // Match complete
};

// Match configuration
struct MatchConfig {
    // Teams
    std::size_t teamCount{4};
    std::size_t maxPlayersPerTeam{4};
    
    // Timing (in server ticks at 30 TPS)
    std::uint32_t startCountdownTicks{300};   // 10 seconds
    std::uint32_t respawnDelayTicks{150};     // 5 seconds
    std::uint32_t endDelayTicks{300};         // 10 seconds after winner
    
    // Combat
    std::uint32_t attackCooldownTicks{15};    // 0.5 seconds
    std::uint32_t regenDelayTicks{120};       // 4 seconds without damage
    std::uint8_t baseMeleeDamage{4};          // Half heart per hit base
    
    // Gameplay
    float itemPickupRadius{1.5f};
    bool friendlyFire{false};
    bool keepInventoryOnDeath{false};
};

// Central game state for BedWars match
class GameState {
public:
    GameState();
    ~GameState() = default;
    
    // Non-copyable
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;
    
    // === Initialization ===
    
    // Initialize for a new match
    void init(const MatchConfig& config);
    
    // Reset for new match (keep players)
    void reset();
    
    // === Player Management ===
    
    // Add player to match
    PlayerState* add_player(engine::PlayerId playerId, const std::string& name);
    
    // Remove player from match
    void remove_player(engine::PlayerId playerId);
    
    // Get player state
    PlayerState* get_player(engine::PlayerId playerId);
    const PlayerState* get_player(engine::PlayerId playerId) const;
    
    // Get all players
    std::vector<PlayerState>& players() { return players_; }
    const std::vector<PlayerState>& players() const { return players_; }
    
    // === Team Management ===
    
    // Assign player to team
    bedwars::game::TeamId assign_player_to_team(engine::PlayerId playerId,
                                                bedwars::game::TeamId preferred = bedwars::game::Teams::None);
    
    // Get team manager
    TeamManager& teams() { return teamManager_; }
    const TeamManager& teams() const { return teamManager_; }
    
    // Get beds
    std::vector<Bed>& beds() { return beds_; }
    const std::vector<Bed>& beds() const { return beds_; }
    
    // Find bed by team
    Bed* get_team_bed(bedwars::game::TeamId teamId);
    
    // === Generator/Items ===
    
    GeneratorManager& generators() { return generatorManager_; }
    const GeneratorManager& generators() const { return generatorManager_; }
    
    // === Match Flow ===
    
    MatchPhase phase() const { return phase_; }
    void set_phase(MatchPhase phase) { phase_ = phase; }
    
    std::uint64_t match_tick() const { return matchTick_; }
    
    // Update game state (called every server tick)
    void update(float deltaTime, std::uint64_t serverTick);
    
    // Start the match
    void start_match();
    
    // End the match
    void end_match(bedwars::game::TeamId winner);
    
    // Check win condition
    void check_win_condition();
    
    // === Combat ===
    
    // Process melee attack, returns damage dealt
    std::uint8_t process_attack(engine::PlayerId attacker, 
                                engine::PlayerId target,
                                std::uint64_t tick);
    
    // Process player death
    void process_death(engine::PlayerId playerId, std::uint64_t tick);
    
    // Process respawn request
    bool try_respawn(engine::PlayerId playerId, std::uint64_t tick);
    
    // === Bed ===
    
    // Process bed destruction
    void process_bed_destruction(int x, int y, int z, 
                                  engine::PlayerId destroyer,
                                  std::uint64_t tick);
    
    // Check if position is a bed
    Bed* get_bed_at(int x, int y, int z);
    
    // === Item Pickup ===
    
    // Try to pick up items near player
    void process_item_pickup(engine::PlayerId playerId);
    
    // === Events (callbacks for networking) ===
    
    std::function<void(engine::PlayerId, const std::string&)> onPlayerMessage;
    std::function<void(const std::string&)> onBroadcast;
    std::function<void(engine::PlayerId, std::uint8_t, std::uint8_t)> onHealthChanged;
    std::function<void(engine::PlayerId, engine::PlayerId)> onPlayerKilled;
    std::function<void(engine::PlayerId, float, float, float)> onPlayerRespawned;
    std::function<void(bedwars::game::TeamId, engine::PlayerId)> onBedDestroyed;
    std::function<void(bedwars::game::TeamId)> onTeamEliminated;
    std::function<void(bedwars::game::TeamId)> onMatchEnded;
    std::function<void(const DroppedItem&)> onItemSpawned;
    std::function<void(EntityId)> onItemPickedUp;
    
    // === Config Access ===
    
    const MatchConfig& config() const { return config_; }

private:
    void update_regeneration(float deltaTime, std::uint64_t tick);
    void update_respawns(std::uint64_t tick);
    
    MatchConfig config_;
    MatchPhase phase_{MatchPhase::Waiting};
    
    std::uint64_t matchTick_{0};
    std::uint64_t phaseStartTick_{0};
    
    std::vector<PlayerState> players_;
    TeamManager teamManager_;
    GeneratorManager generatorManager_;
    std::vector<Bed> beds_;
    
    bedwars::game::TeamId winner_{bedwars::game::Teams::None};
};

} // namespace bedwars::server::game
