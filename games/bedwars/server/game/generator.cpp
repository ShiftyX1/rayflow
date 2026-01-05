#include "generator.hpp"

#include <algorithm>
#include <cmath>

namespace bedwars::server::game {

// === Generator ===

bool Generator::update(float deltaTime) {
    if (!isActive) return false;
    
    // Check if at max items
    if (currentItems >= maxItems) {
        return false;
    }
    
    timeUntilSpawn -= deltaTime;
    
    if (timeUntilSpawn <= 0.0f) {
        timeUntilSpawn = effective_interval();
        return true;  // Spawn item
    }
    
    return false;
}

float Generator::effective_interval() const {
    // Each upgrade level reduces interval by ~25%
    float multiplier = 1.0f;
    switch (upgradeLevel) {
        case 1: multiplier = 0.75f; break;
        case 2: multiplier = 0.5f;  break;
        case 3: multiplier = 0.375f; break;
        case 4: multiplier = 0.25f; break;
        default: break;
    }
    return spawnInterval * multiplier;
}

bedwars::game::ItemType Generator::item_type() const {
    return bedwars::game::generator_item_type(tier);
}

void Generator::on_item_picked_up() {
    if (currentItems > 0) {
        --currentItems;
    }
}

void Generator::on_item_removed() {
    if (currentItems > 0) {
        --currentItems;
    }
}

// === DroppedItem ===

bool DroppedItem::update(float deltaTime) {
    if (!isActive) return true;
    
    if (pickupDelay > 0.0f) {
        pickupDelay -= deltaTime;
    }
    
    lifetime += deltaTime;
    
    // Check for despawn (if maxLifetime > 0)
    if (maxLifetime > 0.0f && lifetime >= maxLifetime) {
        isActive = false;
        return true;
    }
    
    return false;
}

bool DroppedItem::can_pickup() const {
    return isActive && pickupDelay <= 0.0f;
}

// === GeneratorManager ===

EntityId GeneratorManager::create_generator(float x, float y, float z, 
                                             bedwars::game::GeneratorTier tier,
                                             bedwars::game::TeamId ownerTeam) {
    Generator gen;
    gen.id = next_id();
    gen.x = x;
    gen.y = y;
    gen.z = z;
    gen.tier = tier;
    gen.spawnInterval = bedwars::game::default_spawn_interval(tier);
    gen.timeUntilSpawn = gen.spawnInterval;
    gen.maxItems = bedwars::game::generator_max_items(tier);
    gen.ownerTeam = ownerTeam;
    gen.isActive = true;
    
    generators_.push_back(gen);
    return gen.id;
}

Generator* GeneratorManager::get_generator(EntityId id) {
    for (auto& g : generators_) {
        if (g.id == id) return &g;
    }
    return nullptr;
}

const Generator* GeneratorManager::get_generator(EntityId id) const {
    for (const auto& g : generators_) {
        if (g.id == id) return &g;
    }
    return nullptr;
}

std::vector<DroppedItem> GeneratorManager::update_generators(float deltaTime) {
    std::vector<DroppedItem> spawned;
    
    for (auto& gen : generators_) {
        if (gen.update(deltaTime)) {
            // Spawn item
            DroppedItem item;
            item.id = next_id();
            item.x = gen.x;
            item.y = gen.y;
            item.z = gen.z;
            item.itemType = gen.item_type();
            item.count = 1;
            item.sourceGenerator = gen.id;
            item.maxLifetime = 0.0f;  // Generator items don't despawn
            item.isActive = true;
            
            items_.push_back(item);
            gen.currentItems++;
            
            spawned.push_back(item);
        }
    }
    
    return spawned;
}

void GeneratorManager::upgrade_team_generators(bedwars::game::TeamId team, std::uint8_t level) {
    for (auto& gen : generators_) {
        if (gen.ownerTeam == team) {
            gen.upgradeLevel = level;
        }
    }
}

EntityId GeneratorManager::spawn_item(float x, float y, float z, 
                                       bedwars::game::ItemType type, 
                                       std::uint16_t count,
                                       EntityId sourceGenerator) {
    DroppedItem item;
    item.id = next_id();
    item.x = x;
    item.y = y;
    item.z = z;
    item.itemType = type;
    item.count = count;
    item.sourceGenerator = sourceGenerator;
    item.maxLifetime = 300.0f;  // 5 minutes for player-dropped items
    item.isActive = true;
    
    items_.push_back(item);
    return item.id;
}

DroppedItem* GeneratorManager::get_item(EntityId id) {
    for (auto& i : items_) {
        if (i.id == id) return &i;
    }
    return nullptr;
}

const DroppedItem* GeneratorManager::get_item(EntityId id) const {
    for (const auto& i : items_) {
        if (i.id == id) return &i;
    }
    return nullptr;
}

void GeneratorManager::remove_item(EntityId id) {
    auto it = std::find_if(items_.begin(), items_.end(),
        [id](const DroppedItem& i) { return i.id == id; });
    
    if (it != items_.end()) {
        // Notify source generator
        if (it->sourceGenerator != 0) {
            if (auto* gen = get_generator(it->sourceGenerator)) {
                gen->on_item_removed();
            }
        }
        items_.erase(it);
    }
}

std::vector<EntityId> GeneratorManager::update_items(float deltaTime) {
    std::vector<EntityId> despawned;
    
    for (auto& item : items_) {
        if (item.update(deltaTime)) {
            despawned.push_back(item.id);
        }
    }
    
    // Remove despawned items
    for (auto id : despawned) {
        remove_item(id);
    }
    
    return despawned;
}

std::vector<EntityId> GeneratorManager::find_items_near(float x, float y, float z, float radius) const {
    std::vector<EntityId> result;
    float r2 = radius * radius;
    
    for (const auto& item : items_) {
        if (!item.can_pickup()) continue;
        
        float dx = item.x - x;
        float dy = item.y - y;
        float dz = item.z - z;
        float dist2 = dx*dx + dy*dy + dz*dz;
        
        if (dist2 <= r2) {
            result.push_back(item.id);
        }
    }
    
    return result;
}

void GeneratorManager::clear() {
    generators_.clear();
    items_.clear();
    nextId_ = 1;
}

} // namespace bedwars::server::game
