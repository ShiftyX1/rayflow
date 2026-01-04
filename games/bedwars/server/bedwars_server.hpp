#pragma once

#include <engine/core/game_interface.hpp>
#include "../shared/protocol/messages.hpp"
#include "physics_utils.hpp"

// Use shared voxel types from engine
#include <engine/modules/voxel/shared/block.hpp>

#include <memory>
#include <unordered_map>
#include <vector>
#include <array>

// Forward declare terrain (now in bedwars)
namespace bedwars::voxel {
class Terrain;
}

namespace bedwars::server {

// ============================================================================
// Match Phases
// ============================================================================

enum class MatchPhase : std::uint8_t {
    Waiting,        // Waiting for players (min players not reached)
    Starting,       // Countdown before start
    InProgress,     // Match running
    Ending,         // Match ending (winner determined)
    Finished        // Match complete
};

// ============================================================================
// Match Configuration
// ============================================================================

struct MatchConfig {
    // Teams
    std::size_t teamCount{4};
    std::size_t maxPlayersPerTeam{4};
    std::size_t minPlayersToStart{2};
    
    // Timing (in seconds)
    float startCountdown{10.0f};
    float respawnDelay{5.0f};
    float endDelay{10.0f};
    
    // Combat
    float attackCooldown{0.5f};
    float regenDelay{4.0f};
    std::uint8_t baseMeleeDamage{4};  // Half hearts
    
    // Gameplay
    float itemPickupRadius{1.5f};
    bool friendlyFire{false};
};

// ============================================================================
// Team State
// ============================================================================

struct TeamState {
    proto::TeamId id{proto::Teams::None};
    std::string name;
    
    // Spawn point
    float spawnX{0.0f};
    float spawnY{64.0f};
    float spawnZ{0.0f};
    
    // Bed
    int bedX{0}, bedY{64}, bedZ{0};
    bool bedAlive{true};
    
    // Players
    std::vector<engine::PlayerId> members;
    
    bool is_eliminated() const;
};

// ============================================================================
// Generator State
// ============================================================================

struct GeneratorState {
    std::uint32_t id{0};
    
    // Position
    float x{0.0f}, y{0.0f}, z{0.0f};
    
    // Type: 0=iron, 1=gold, 2=diamond, 3=emerald
    std::uint8_t tier{0};
    
    // Timing
    float spawnInterval{1.0f};
    float timeUntilSpawn{0.0f};
    
    // Team ownership (Teams::None = shared center generator)
    proto::TeamId ownerTeam{proto::Teams::None};
    
    bool isActive{true};
};

// ============================================================================
// Dropped Item
// ============================================================================

struct DroppedItemState {
    std::uint32_t entityId{0};
    proto::ItemType itemType{proto::ItemType::None};
    float x{0.0f}, y{0.0f}, z{0.0f};
    std::uint16_t count{1};
    float lifetime{0.0f};
    float pickupDelay{0.0f};
    bool active{true};
};

// ============================================================================
// BedWarsServer - Implements engine::IGameServer
// ============================================================================

class BedWarsServer : public engine::IGameServer {
public:
    // --- Player state ---
    struct PlayerState {
        std::string name;
        bool joined{false};
        bool handshakeComplete{false};
        
        // Position (authoritative)
        float px{50.0f};
        float py{80.0f};
        float pz{50.0f};
        
        // Velocity
        float vx{0.0f};
        float vy{0.0f};
        float vz{0.0f};
        
        // Physics state
        bool onGround{false};
        bool lastJumpHeld{false};
        
        // Input
        proto::InputFrame lastInput{};
        
        // Combat
        std::uint8_t hp{20};
        std::uint8_t maxHp{20};
        float lastDamageTaken{0.0f};  // Time since last damage (for regen)
        float attackCooldown{0.0f};
        
        // Respawn
        bool alive{true};
        float respawnTimer{0.0f};
        
        // Team
        proto::TeamId team{proto::Teams::None};
        bool hasBed{true};
        
        // Simple inventory: count per item type
        std::unordered_map<proto::ItemType, std::uint16_t> inventory;
    };
    
    struct Options {
        bool editorCameraMode{false};   // Free-fly camera for map editor
        bool loadMapTemplate{true};     // Load .rfmap on startup
        bool autoStartMatch{true};      // Auto-start when min players reached
        std::string mapName;            // Map file to load (empty = most recent)
    };
    
    explicit BedWarsServer(std::uint32_t seed = 12345);
    BedWarsServer(std::uint32_t seed, const Options& opts);
    ~BedWarsServer() override;

    // --- IGameServer implementation ---
    
    void on_init(engine::IEngineServices& engine) override;
    void on_shutdown() override;
    void on_tick(float dt) override;
    void on_player_connect(engine::PlayerId id) override;
    void on_player_disconnect(engine::PlayerId id) override;
    void on_player_message(engine::PlayerId id, std::span<const std::uint8_t> data) override;

private:
    // --- Message handlers ---
    void handle_client_hello(engine::PlayerId id, const proto::ClientHello& msg);
    void handle_join_match(engine::PlayerId id, const proto::JoinMatch& msg);
    void handle_input_frame(engine::PlayerId id, const proto::InputFrame& msg);
    void handle_try_place_block(engine::PlayerId id, const proto::TryPlaceBlock& msg);
    void handle_try_break_block(engine::PlayerId id, const proto::TryBreakBlock& msg);
    void handle_try_set_block(engine::PlayerId id, const proto::TrySetBlock& msg);
    void handle_try_export_map(engine::PlayerId id, const proto::TryExportMap& msg);
    
    // --- Match flow ---
    void update_match_phase(float dt);
    void start_match();
    void end_match(proto::TeamId winner);
    void check_win_condition();
    proto::TeamId assign_team(engine::PlayerId playerId);
    
    // --- Combat ---
    void process_damage(engine::PlayerId target, std::uint8_t damage, engine::PlayerId attacker);
    void process_death(engine::PlayerId playerId, engine::PlayerId killerId);
    void process_respawn(engine::PlayerId playerId);
    void update_regeneration(float dt);
    void update_respawns(float dt);
    
    // --- Generators & Items ---
    void update_generators(float dt);
    void spawn_item(const GeneratorState& gen);
    void update_items(float dt);
    void process_item_pickup(engine::PlayerId playerId);
    std::uint32_t next_entity_id();
    
    // --- Beds ---
    void process_bed_break(int x, int y, int z, engine::PlayerId breakerId);
    TeamState* get_team_at_bed(int x, int y, int z);
    
    // --- Helpers ---
    void send_message(engine::PlayerId id, const proto::Message& msg);
    void broadcast_message(const proto::Message& msg);
    void send_chunk_data(engine::PlayerId id, int chunkX, int chunkZ);
    
    // --- Physics ---
    void simulate_player(PlayerState& player, float dt);
    void simulate_editor_camera(PlayerState& player, float dt);
    bool check_collision_at(float px, float py, float pz) const;
    
    // --- Block validation ---
    bool validate_place_block(engine::PlayerId id, const proto::TryPlaceBlock& msg, proto::RejectReason& reason);
    bool validate_break_block(engine::PlayerId id, const proto::TryBreakBlock& msg, proto::RejectReason& reason);
    void broadcast_neighbor_updates(int x, int y, int z);
    
    // --- State ---
    engine::IEngineServices* engine_{nullptr};
    std::unordered_map<engine::PlayerId, PlayerState> players_;
    
    Options opts_{};
    MatchConfig matchConfig_{};
    MatchPhase matchPhase_{MatchPhase::Waiting};
    float phaseTimer_{0.0f};
    
    std::uint32_t worldSeed_{12345};
    std::unique_ptr<::bedwars::voxel::Terrain> terrain_;
    
    // Teams (indexed by team ID - 1, since Teams::None = 0)
    std::array<TeamState, 4> teams_;
    std::size_t nextTeamAssign_{0};  // Round-robin assignment
    
    // Generators
    std::vector<GeneratorState> generators_;
    
    // Dropped items
    std::vector<DroppedItemState> droppedItems_;
    std::uint32_t nextEntityId_{1};
    
    // Map template info
    bool hasMapTemplate_{false};
    std::string mapId_;
    std::uint32_t mapVersion_{0};
    float mapCenterX_{0.0f};
    float mapCenterZ_{0.0f};
    
    // Track modified blocks (for sending delta to new clients)
    struct ModifiedBlock {
        int x, y, z;
        shared::voxel::BlockType type;
        std::uint8_t state;
    };
    std::vector<ModifiedBlock> modifiedBlocks_;
};

} // namespace bedwars::server
