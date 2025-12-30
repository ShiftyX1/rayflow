#pragma once

#include <cstdint>

namespace shared::game {

// Item type enumeration for BedWars resources and items
enum class ItemType : std::uint16_t {
    None = 0,
    
    // === Resources (currency) ===
    Iron = 1,
    Gold = 2,
    Diamond = 3,
    Emerald = 4,
    
    // === Blocks ===
    Wool = 100,
    Wood = 101,
    EndStone = 102,
    Glass = 103,
    Obsidian = 104,
    
    // === Weapons ===
    WoodSword = 200,
    StoneSword = 201,
    IronSword = 202,
    DiamondSword = 203,
    
    // === Tools ===
    WoodPickaxe = 300,
    IronPickaxe = 301,
    DiamondPickaxe = 302,
    GoldPickaxe = 303,
    Shears = 310,
    
    // === Armor ===
    LeatherArmor = 400,
    ChainArmor = 401,
    IronArmor = 402,
    DiamondArmor = 403,
    
    // === Utility ===
    Bow = 500,
    Arrow = 501,
    Fireball = 502,
    TNT = 503,
    EnderPearl = 504,
    WaterBucket = 505,
    Sponge = 506,
    GoldenApple = 507,
    BridgeEgg = 508,
    PopupTower = 509,
    
    // === Special ===
    BedBug = 600,       // Silverfish egg
    DreamDefender = 601, // Iron golem spawn
    
    MaxItemType
};

// Check if item is a resource (currency)
inline bool is_resource(ItemType type) {
    auto v = static_cast<std::uint16_t>(type);
    return v >= 1 && v <= 4;
}

// Check if item is a block
inline bool is_block(ItemType type) {
    auto v = static_cast<std::uint16_t>(type);
    return v >= 100 && v < 200;
}

// Check if item is a weapon
inline bool is_weapon(ItemType type) {
    auto v = static_cast<std::uint16_t>(type);
    return v >= 200 && v < 300;
}

// Check if item is a tool
inline bool is_tool(ItemType type) {
    auto v = static_cast<std::uint16_t>(type);
    return v >= 300 && v < 400;
}

// Check if item is armor
inline bool is_armor(ItemType type) {
    auto v = static_cast<std::uint16_t>(type);
    return v >= 400 && v < 500;
}

// Get item name string
inline const char* item_name(ItemType type) {
    switch (type) {
        case ItemType::Iron:          return "Iron";
        case ItemType::Gold:          return "Gold";
        case ItemType::Diamond:       return "Diamond";
        case ItemType::Emerald:       return "Emerald";
        case ItemType::Wool:          return "Wool";
        case ItemType::Wood:          return "Wood";
        case ItemType::EndStone:      return "End Stone";
        case ItemType::Glass:         return "Glass";
        case ItemType::Obsidian:      return "Obsidian";
        case ItemType::WoodSword:     return "Wooden Sword";
        case ItemType::StoneSword:    return "Stone Sword";
        case ItemType::IronSword:     return "Iron Sword";
        case ItemType::DiamondSword:  return "Diamond Sword";
        case ItemType::WoodPickaxe:   return "Wooden Pickaxe";
        case ItemType::IronPickaxe:   return "Iron Pickaxe";
        case ItemType::DiamondPickaxe:return "Diamond Pickaxe";
        case ItemType::GoldPickaxe:   return "Golden Pickaxe";
        case ItemType::Shears:        return "Shears";
        case ItemType::LeatherArmor:  return "Leather Armor";
        case ItemType::ChainArmor:    return "Chain Armor";
        case ItemType::IronArmor:     return "Iron Armor";
        case ItemType::DiamondArmor:  return "Diamond Armor";
        case ItemType::Bow:           return "Bow";
        case ItemType::Arrow:         return "Arrow";
        case ItemType::Fireball:      return "Fireball";
        case ItemType::TNT:           return "TNT";
        case ItemType::EnderPearl:    return "Ender Pearl";
        case ItemType::WaterBucket:   return "Water Bucket";
        case ItemType::Sponge:        return "Sponge";
        case ItemType::GoldenApple:   return "Golden Apple";
        case ItemType::BridgeEgg:     return "Bridge Egg";
        case ItemType::PopupTower:    return "Popup Tower";
        case ItemType::BedBug:        return "Bed Bug";
        case ItemType::DreamDefender: return "Dream Defender";
        default:                      return "Unknown";
    }
}

// Max stack size for an item type
inline std::uint16_t max_stack_size(ItemType type) {
    switch (type) {
        case ItemType::Iron:
        case ItemType::Gold:
        case ItemType::Diamond:
        case ItemType::Emerald:
            return 64;
            
        case ItemType::Wool:
        case ItemType::Wood:
        case ItemType::EndStone:
        case ItemType::Glass:
            return 64;
            
        case ItemType::Obsidian:
            return 16;
            
        case ItemType::Arrow:
            return 64;
            
        case ItemType::EnderPearl:
        case ItemType::Fireball:
        case ItemType::GoldenApple:
        case ItemType::BridgeEgg:
        case ItemType::PopupTower:
        case ItemType::BedBug:
            return 16;
            
        case ItemType::TNT:
        case ItemType::WaterBucket:
        case ItemType::Sponge:
            return 1;
            
        // Weapons, tools, armor don't stack
        default:
            if (is_weapon(type) || is_tool(type) || is_armor(type)) {
                return 1;
            }
            return 64;
    }
}

} // namespace shared::game
