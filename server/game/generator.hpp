#pragma once

#include "../../shared/game/generator_types.hpp"
#include "../../shared/game/item_types.hpp"
#include "../../shared/game/team_types.hpp"

#include <cstdint>
#include <vector>

namespace server::game {

// Unique ID for generators and dropped items
using EntityId = std::uint32_t;

// Resource generator entity
struct Generator {
    EntityId id{0};
    
    // Position
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    
    // Type and timing
    shared::game::GeneratorTier tier{shared::game::GeneratorTier::Iron};
    float spawnInterval{1.0f};  // Seconds between spawns
    float timeUntilSpawn{0.0f}; // Countdown
    
    // Configuration
    std::uint8_t maxItems{48};      // Max items on ground near generator
    std::uint8_t currentItems{0};   // Current count
    float pickupRadius{2.0f};       // Radius for item pickup
    
    // Team ownership (0 = shared/center generator)
    shared::game::TeamId ownerTeam{0};
    
    // Upgrades (for team-owned generators)
    std::uint8_t upgradeLevel{0};   // Affects spawn interval
    
    bool isActive{true};
    
    // === Methods ===
    
    // Update generator, returns true if item should spawn
    bool update(float deltaTime);
    
    // Get current spawn interval (accounting for upgrades)
    float effective_interval() const;
    
    // Item type this generator produces
    shared::game::ItemType item_type() const;
    
    // Called when item is picked up from this generator's area
    void on_item_picked_up();
    
    // Called when item despawns or is destroyed
    void on_item_removed();
};

// Dropped item entity (on ground)
struct DroppedItem {
    EntityId id{0};
    
    // Position
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    
    // Item info
    shared::game::ItemType itemType{shared::game::ItemType::None};
    std::uint16_t count{1};
    
    // Source generator (0 if dropped by player)
    EntityId sourceGenerator{0};
    
    // Lifetime
    float lifetime{0.0f};           // Time since spawn
    float maxLifetime{300.0f};      // Despawn after 5 minutes (0 = never)
    
    // Pickup delay (prevent instant re-pickup after death drop)
    float pickupDelay{0.0f};
    
    bool isActive{true};
    
    // === Methods ===
    
    // Update item, returns true if should despawn
    bool update(float deltaTime);
    
    // Check if item can be picked up
    bool can_pickup() const;
};

// Manages all generators and dropped items
class GeneratorManager {
public:
    GeneratorManager() = default;
    
    // === Generators ===
    
    // Create a generator at position
    EntityId create_generator(float x, float y, float z, 
                              shared::game::GeneratorTier tier,
                              shared::game::TeamId ownerTeam = 0);
    
    // Get generator by ID
    Generator* get_generator(EntityId id);
    const Generator* get_generator(EntityId id) const;
    
    // Update all generators, returns list of new item spawns
    std::vector<DroppedItem> update_generators(float deltaTime);
    
    // Upgrade team's generators
    void upgrade_team_generators(shared::game::TeamId team, std::uint8_t level);
    
    // === Dropped Items ===
    
    // Spawn a dropped item
    EntityId spawn_item(float x, float y, float z, 
                        shared::game::ItemType type, 
                        std::uint16_t count = 1,
                        EntityId sourceGenerator = 0);
    
    // Get dropped item by ID
    DroppedItem* get_item(EntityId id);
    const DroppedItem* get_item(EntityId id) const;
    
    // Remove item (picked up or despawned)
    void remove_item(EntityId id);
    
    // Update all items, returns list of despawned item IDs
    std::vector<EntityId> update_items(float deltaTime);
    
    // Find items near position
    std::vector<EntityId> find_items_near(float x, float y, float z, float radius) const;
    
    // === Utility ===
    
    // Clear all generators and items (new match)
    void clear();
    
    // Access collections
    std::vector<Generator>& generators() { return generators_; }
    const std::vector<Generator>& generators() const { return generators_; }
    
    std::vector<DroppedItem>& items() { return items_; }
    const std::vector<DroppedItem>& items() const { return items_; }

private:
    EntityId next_id() { return nextId_++; }
    
    std::vector<Generator> generators_;
    std::vector<DroppedItem> items_;
    EntityId nextId_{1};
};

} // namespace server::game
