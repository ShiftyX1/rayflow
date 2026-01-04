#pragma once

#include "../../shared/game/item_types.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace bedwars::server::game {

// Single inventory slot
struct InventorySlot {
    bedwars::game::ItemType item{bedwars::game::ItemType::None};
    std::uint16_t count{0};
    
    bool is_empty() const { return item == bedwars::game::ItemType::None || count == 0; }
    
    void clear() {
        item = bedwars::game::ItemType::None;
        count = 0;
    }
};

// Player inventory for BedWars
// Simplified: hotbar + armor, no chest inventory
class Inventory {
public:
    static constexpr std::size_t HotbarSize = 9;
    static constexpr std::size_t ArmorSlots = 4; // Helmet, Chest, Legs, Boots
    
    Inventory() = default;
    
    // === Resource management (iron/gold/diamond/emerald) ===
    
    // Get current count of a resource type
    std::uint32_t get_resource_count(bedwars::game::ItemType type) const;
    
    // Add resources, returns amount actually added
    std::uint32_t add_resource(bedwars::game::ItemType type, std::uint32_t amount);
    
    // Remove resources, returns true if successful
    bool remove_resource(bedwars::game::ItemType type, std::uint32_t amount);
    
    // Check if player has enough resources
    bool has_resource(bedwars::game::ItemType type, std::uint32_t amount) const;
    
    // === Hotbar item management ===
    
    // Get item at hotbar slot (0-8)
    const InventorySlot& get_hotbar_slot(std::size_t slot) const;
    
    // Set item at hotbar slot
    void set_hotbar_slot(std::size_t slot, bedwars::game::ItemType item, std::uint16_t count = 1);
    
    // Add item to first available hotbar slot, returns slot index or -1 if full
    int add_to_hotbar(bedwars::game::ItemType item, std::uint16_t count = 1);
    
    // Remove item from hotbar slot
    void remove_from_hotbar(std::size_t slot, std::uint16_t count = 1);
    
    // Find first slot containing item type, returns -1 if not found
    int find_item(bedwars::game::ItemType item) const;
    
    // Check if hotbar has item
    bool has_item(bedwars::game::ItemType item) const;
    
    // === Armor management ===
    
    enum ArmorSlotIndex : std::size_t {
        Helmet = 0,
        Chestplate = 1,
        Leggings = 2,
        Boots = 3
    };
    
    // Get current armor in slot
    bedwars::game::ItemType get_armor(ArmorSlotIndex slot) const;
    
    // Set armor in slot (old armor is lost)
    void set_armor(ArmorSlotIndex slot, bedwars::game::ItemType item);
    
    // Get total armor tier (for damage reduction calculation)
    std::uint8_t get_armor_tier() const;
    
    // === Selected slot ===
    
    std::size_t selected_slot() const { return selectedSlot_; }
    void set_selected_slot(std::size_t slot);
    
    const InventorySlot& get_selected_item() const { return hotbar_[selectedSlot_]; }
    
    // === Utility ===
    
    // Clear entire inventory
    void clear();
    
    // Clear only hotbar (keep armor)
    void clear_hotbar();
    
    // Give default starting items for BedWars
    void give_starting_items();

private:
    std::array<InventorySlot, HotbarSize> hotbar_{};
    std::array<bedwars::game::ItemType, ArmorSlots> armor_{};
    
    // Resource counts stored separately for efficiency
    std::uint32_t iron_{0};
    std::uint32_t gold_{0};
    std::uint32_t diamond_{0};
    std::uint32_t emerald_{0};
    
    std::size_t selectedSlot_{0};
};

} // namespace bedwars::server::game
