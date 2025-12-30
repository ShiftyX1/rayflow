#pragma once

#include "../../shared/game/team_types.hpp"
#include "player_state.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace server::game {

// Spawn point for a team
struct SpawnPoint {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float yaw{0.0f};  // Facing direction
};

// Team upgrades (purchased from shop, affect all team members)
struct TeamUpgrades {
    std::uint8_t sharpness{0};     // Sword damage boost (0-3)
    std::uint8_t protection{0};    // Armor protection (0-4)
    std::uint8_t haste{0};         // Mining speed (0-2)
    std::uint8_t forge{0};         // Resource generation speed (0-4)
    
    bool healPool{false};          // Passive healing on island
    bool dragonBuff{false};        // End-game dragon buff
    
    // Traps (one-time use)
    std::uint8_t alarmTraps{0};
    std::uint8_t blindTraps{0};
    std::uint8_t slowTraps{0};
    std::uint8_t minerFatigueTraps{0};
};

// Team state in BedWars match
struct Team {
    shared::game::TeamId id{shared::game::Teams::None};
    std::string name;
    shared::game::TeamColor color;
    
    // Bed state
    bool bedAlive{true};
    int bedX{0}, bedY{0}, bedZ{0};  // Bed position (for destruction check)
    
    // Spawn
    SpawnPoint spawn;
    
    // Members
    std::vector<shared::proto::PlayerId> memberIds;
    
    // Upgrades
    TeamUpgrades upgrades;
    
    // Status
    bool isEliminated{false};  // All members dead + bed destroyed
    
    // === Methods ===
    
    bool has_member(shared::proto::PlayerId playerId) const;
    void add_member(shared::proto::PlayerId playerId);
    void remove_member(shared::proto::PlayerId playerId);
    std::size_t alive_member_count(const std::vector<PlayerState>& allPlayers) const;
    
    // Check if team is eliminated (no alive members and bed destroyed)
    bool check_eliminated(const std::vector<PlayerState>& allPlayers);
    
    void destroy_bed();
    
    void reset();
};

// Manages all teams in a match
class TeamManager {
public:
    TeamManager() = default;
    
    // Initialize teams for a match (2, 4, or 8 teams typically)
    void init_teams(std::size_t teamCount);
    
    // Get team by ID
    Team* get_team(shared::game::TeamId id);
    const Team* get_team(shared::game::TeamId id) const;
    
    // Get team for a player
    Team* get_player_team(shared::proto::PlayerId playerId);
    const Team* get_player_team(shared::proto::PlayerId playerId) const;
    
    // Assign player to team (auto-balance or specific)
    shared::game::TeamId assign_player(shared::proto::PlayerId playerId, 
                                        shared::game::TeamId preferredTeam = shared::game::Teams::None);
    
    // Remove player from their team
    void remove_player(shared::proto::PlayerId playerId);
    
    // Get team with fewest members (for auto-balance)
    shared::game::TeamId get_smallest_team() const;
    
    // Count non-eliminated teams
    std::size_t alive_team_count() const;
    
    // Get winning team (if only one remains)
    shared::game::TeamId get_winner() const;
    
    // Update elimination status for all teams
    void update_eliminations(const std::vector<PlayerState>& allPlayers);
    
    // Access all teams
    std::vector<Team>& teams() { return teams_; }
    const std::vector<Team>& teams() const { return teams_; }
    
    // Reset all teams for new match
    void reset();

private:
    std::vector<Team> teams_;
};

} // namespace server::game
