#include "team.hpp"

#include <algorithm>

namespace server::game {

// === Team ===

bool Team::has_member(shared::proto::PlayerId playerId) const {
    return std::find(memberIds.begin(), memberIds.end(), playerId) != memberIds.end();
}

void Team::add_member(shared::proto::PlayerId playerId) {
    if (!has_member(playerId)) {
        memberIds.push_back(playerId);
    }
}

void Team::remove_member(shared::proto::PlayerId playerId) {
    memberIds.erase(
        std::remove(memberIds.begin(), memberIds.end(), playerId),
        memberIds.end()
    );
}

std::size_t Team::alive_member_count(const std::vector<PlayerState>& allPlayers) const {
    std::size_t count = 0;
    for (auto pid : memberIds) {
        for (const auto& p : allPlayers) {
            if (p.playerId == pid && p.isAlive) {
                ++count;
                break;
            }
        }
    }
    return count;
}

bool Team::check_eliminated(const std::vector<PlayerState>& allPlayers) {
    if (isEliminated) return true;
    
    // Team is eliminated if bed is destroyed AND no members alive
    if (!bedAlive && alive_member_count(allPlayers) == 0) {
        isEliminated = true;
    }
    
    return isEliminated;
}

void Team::destroy_bed() {
    bedAlive = false;
}

void Team::reset() {
    bedAlive = true;
    isEliminated = false;
    upgrades = TeamUpgrades{};
    // Keep memberIds - players stay on their teams
}

// === TeamManager ===

void TeamManager::init_teams(std::size_t teamCount) {
    teams_.clear();
    teams_.reserve(teamCount);
    
    // Create teams with predefined IDs and colors
    const shared::game::TeamId teamIds[] = {
        shared::game::Teams::Red,
        shared::game::Teams::Blue,
        shared::game::Teams::Green,
        shared::game::Teams::Yellow,
        shared::game::Teams::Aqua,
        shared::game::Teams::White,
        shared::game::Teams::Pink,
        shared::game::Teams::Gray,
    };
    
    for (std::size_t i = 0; i < teamCount && i < 8; ++i) {
        Team team;
        team.id = teamIds[i];
        team.name = shared::game::team_name(team.id);
        team.color = shared::game::TeamColor::from_team_id(team.id);
        teams_.push_back(std::move(team));
    }
}

Team* TeamManager::get_team(shared::game::TeamId id) {
    for (auto& t : teams_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const Team* TeamManager::get_team(shared::game::TeamId id) const {
    for (const auto& t : teams_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

Team* TeamManager::get_player_team(shared::proto::PlayerId playerId) {
    for (auto& t : teams_) {
        if (t.has_member(playerId)) return &t;
    }
    return nullptr;
}

const Team* TeamManager::get_player_team(shared::proto::PlayerId playerId) const {
    for (const auto& t : teams_) {
        if (t.has_member(playerId)) return &t;
    }
    return nullptr;
}

shared::game::TeamId TeamManager::assign_player(shared::proto::PlayerId playerId, 
                                                 shared::game::TeamId preferredTeam) {
    // First, remove from any existing team
    remove_player(playerId);
    
    Team* targetTeam = nullptr;
    
    if (preferredTeam != shared::game::Teams::None) {
        targetTeam = get_team(preferredTeam);
    }
    
    // If no preference or team not found, auto-balance
    if (!targetTeam) {
        auto smallestId = get_smallest_team();
        targetTeam = get_team(smallestId);
    }
    
    if (targetTeam) {
        targetTeam->add_member(playerId);
        return targetTeam->id;
    }
    
    return shared::game::Teams::None;
}

void TeamManager::remove_player(shared::proto::PlayerId playerId) {
    for (auto& t : teams_) {
        t.remove_member(playerId);
    }
}

shared::game::TeamId TeamManager::get_smallest_team() const {
    if (teams_.empty()) return shared::game::Teams::None;
    
    shared::game::TeamId smallest = teams_[0].id;
    std::size_t smallestSize = teams_[0].memberIds.size();
    
    for (const auto& t : teams_) {
        if (t.memberIds.size() < smallestSize) {
            smallest = t.id;
            smallestSize = t.memberIds.size();
        }
    }
    
    return smallest;
}

std::size_t TeamManager::alive_team_count() const {
    std::size_t count = 0;
    for (const auto& t : teams_) {
        if (!t.isEliminated) ++count;
    }
    return count;
}

shared::game::TeamId TeamManager::get_winner() const {
    shared::game::TeamId winner = shared::game::Teams::None;
    std::size_t aliveCount = 0;
    
    for (const auto& t : teams_) {
        if (!t.isEliminated) {
            winner = t.id;
            ++aliveCount;
        }
    }
    
    // Only return winner if exactly one team remains
    return (aliveCount == 1) ? winner : shared::game::Teams::None;
}

void TeamManager::update_eliminations(const std::vector<PlayerState>& allPlayers) {
    for (auto& t : teams_) {
        t.check_eliminated(allPlayers);
    }
}

void TeamManager::reset() {
    for (auto& t : teams_) {
        t.reset();
    }
}

} // namespace server::game
