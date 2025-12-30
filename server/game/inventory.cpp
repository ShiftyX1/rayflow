#include "inventory.hpp"

#include <algorithm>

namespace server::game {

std::uint32_t Inventory::get_resource_count(shared::game::ItemType type) const {
    switch (type) {
        case shared::game::ItemType::Iron:    return iron_;
        case shared::game::ItemType::Gold:    return gold_;
        case shared::game::ItemType::Diamond: return diamond_;
        case shared::game::ItemType::Emerald: return emerald_;
        default: return 0;
    }
}

std::uint32_t Inventory::add_resource(shared::game::ItemType type, std::uint32_t amount) {
    // Resources are stored in dedicated counters, not slots
    switch (type) {
        case shared::game::ItemType::Iron:
            iron_ += amount;
            return amount;
        case shared::game::ItemType::Gold:
            gold_ += amount;
            return amount;
        case shared::game::ItemType::Diamond:
            diamond_ += amount;
            return amount;
        case shared::game::ItemType::Emerald:
            emerald_ += amount;
            return amount;
        default:
            return 0;
    }
}

bool Inventory::remove_resource(shared::game::ItemType type, std::uint32_t amount) {
    switch (type) {
        case shared::game::ItemType::Iron:
            if (iron_ >= amount) { iron_ -= amount; return true; }
            return false;
        case shared::game::ItemType::Gold:
            if (gold_ >= amount) { gold_ -= amount; return true; }
            return false;
        case shared::game::ItemType::Diamond:
            if (diamond_ >= amount) { diamond_ -= amount; return true; }
            return false;
        case shared::game::ItemType::Emerald:
            if (emerald_ >= amount) { emerald_ -= amount; return true; }
            return false;
        default:
            return false;
    }
}

bool Inventory::has_resource(shared::game::ItemType type, std::uint32_t amount) const {
    return get_resource_count(type) >= amount;
}

const InventorySlot& Inventory::get_hotbar_slot(std::size_t slot) const {
    static const InventorySlot empty{};
    if (slot >= HotbarSize) return empty;
    return hotbar_[slot];
}

void Inventory::set_hotbar_slot(std::size_t slot, shared::game::ItemType item, std::uint16_t count) {
    if (slot >= HotbarSize) return;
    hotbar_[slot].item = item;
    hotbar_[slot].count = count;
}

int Inventory::add_to_hotbar(shared::game::ItemType item, std::uint16_t count) {
    // First, try to stack with existing item
    const auto maxStack = shared::game::max_stack_size(item);
    for (std::size_t i = 0; i < HotbarSize; ++i) {
        if (hotbar_[i].item == item && hotbar_[i].count < maxStack) {
            std::uint16_t canAdd = maxStack - hotbar_[i].count;
            std::uint16_t toAdd = std::min(count, canAdd);
            hotbar_[i].count += toAdd;
            if (toAdd >= count) {
                return static_cast<int>(i);
            }
            count -= toAdd;
        }
    }
    
    // Then, find first empty slot
    for (std::size_t i = 0; i < HotbarSize; ++i) {
        if (hotbar_[i].is_empty()) {
            hotbar_[i].item = item;
            hotbar_[i].count = count;
            return static_cast<int>(i);
        }
    }
    
    return -1; // Inventory full
}

void Inventory::remove_from_hotbar(std::size_t slot, std::uint16_t count) {
    if (slot >= HotbarSize) return;
    
    if (hotbar_[slot].count <= count) {
        hotbar_[slot].clear();
    } else {
        hotbar_[slot].count -= count;
    }
}

int Inventory::find_item(shared::game::ItemType item) const {
    for (std::size_t i = 0; i < HotbarSize; ++i) {
        if (hotbar_[i].item == item) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool Inventory::has_item(shared::game::ItemType item) const {
    return find_item(item) >= 0;
}

shared::game::ItemType Inventory::get_armor(ArmorSlotIndex slot) const {
    if (static_cast<std::size_t>(slot) >= ArmorSlots) {
        return shared::game::ItemType::None;
    }
    return armor_[static_cast<std::size_t>(slot)];
}

void Inventory::set_armor(ArmorSlotIndex slot, shared::game::ItemType item) {
    if (static_cast<std::size_t>(slot) >= ArmorSlots) return;
    armor_[static_cast<std::size_t>(slot)] = item;
}

std::uint8_t Inventory::get_armor_tier() const {
    // Calculate total protection level
    // Leather = 1, Chain = 2, Iron = 3, Diamond = 4
    std::uint8_t total = 0;
    for (auto item : armor_) {
        switch (item) {
            case shared::game::ItemType::LeatherArmor:  total += 1; break;
            case shared::game::ItemType::ChainArmor:    total += 2; break;
            case shared::game::ItemType::IronArmor:     total += 3; break;
            case shared::game::ItemType::DiamondArmor:  total += 4; break;
            default: break;
        }
    }
    return total;
}

void Inventory::set_selected_slot(std::size_t slot) {
    if (slot < HotbarSize) {
        selectedSlot_ = slot;
    }
}

void Inventory::clear() {
    clear_hotbar();
    for (auto& a : armor_) {
        a = shared::game::ItemType::None;
    }
    iron_ = gold_ = diamond_ = emerald_ = 0;
}

void Inventory::clear_hotbar() {
    for (auto& slot : hotbar_) {
        slot.clear();
    }
}

void Inventory::give_starting_items() {
    clear();
    // BedWars starting items: wooden sword
    set_hotbar_slot(0, shared::game::ItemType::WoodSword, 1);
}

} // namespace server::game
