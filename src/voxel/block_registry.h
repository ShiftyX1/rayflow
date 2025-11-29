#ifndef BLOCK_REGISTRY_H
#define BLOCK_REGISTRY_H

#include "texture_atlas.h"
#include <stdint.h>
#include <stdbool.h>

// Instrument types
typedef enum {
    TOOL_NONE = 0,      // Hand or any tool
    TOOL_PICKAXE,       // Pickaxe (stone, ore)
    TOOL_AXE,           // Axe (wood)
    TOOL_SHOVEL,        // Shovel (dirt, sand, gravel)
    TOOL_HOE,           // Hoe (for tilling soil)
    TOOL_SWORD,         // Sword (breaks cobwebs faster, etc.)
    TOOL_SHEARS,        // Shears (leaves, wool)
    TOOL_COUNT
} ToolType;

// Tool levels (material)
typedef enum {
    TOOL_LEVEL_HAND = 0,    // Hand
    TOOL_LEVEL_WOOD = 1,    // Wood
    TOOL_LEVEL_STONE = 2,   // Stone
    TOOL_LEVEL_IRON = 3,    // Iron
    TOOL_LEVEL_DIAMOND = 4, // Diamond
    TOOL_LEVEL_NETHERITE = 5 // Netherite
} ToolLevel;

// Block types (ID)
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_WATER,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_COBBLESTONE,
    BLOCK_PLANKS,
    BLOCK_GLASS,
    BLOCK_BRICK,
    BLOCK_COUNT
} BlockType;

// Texture indices for different block sides
typedef struct {
    int top;      // Top face
    int bottom;   // Bottom face
    int side;     // Side faces (north, south, east, west)
    int north;    // North face (optional, if different from side)
    int south;    // South face
    int east;     // East face
    int west;     // West face
    bool use_individual_sides; // Use individual textures for sides
} BlockTextures;

// Block break properties
typedef struct {
    float hardness;           // Block hardness (seconds to break by hand, -1 = unbreakable)
    ToolType required_tool;   // Required tool type to obtain drop
    ToolLevel required_level; // Minimum tool level
    float tool_multiplier;    // Speed multiplier when using the correct tool
    bool instant_break;       // Instant break (flowers, grass)
} BlockBreakProperties;

// Block properties
typedef struct {
    BlockType type;
    const char* name;
    bool is_solid;        // Solid block (collision)
    bool is_transparent;  // Transparent block (does not block light)
    bool is_opaque;       // Opaque block (blocks light)
    bool is_liquid;       // Liquid
    float light_emission; // Light emission (0.0 - 1.0)
    BlockTextures textures; // Texture indices in the atlas
    BlockBreakProperties break_props; // Break properties
} BlockProperties;

// Block registry
typedef struct {
    BlockProperties blocks[BLOCK_COUNT];
    TextureAtlas* atlas;
    bool is_initialized;
} BlockRegistry;

// Global block registry instance
extern BlockRegistry g_block_registry;

// Block registry initialization
bool block_registry_init(const char* atlas_path);

// Block registration
void block_registry_register(BlockType type, const char* name, 
                             bool is_solid, bool is_transparent, bool is_opaque,
                             bool is_liquid, float light_emission,
                             BlockTextures textures, BlockBreakProperties break_props);

// Helper functions for creating break properties
BlockBreakProperties block_break_default(float hardness);  // Default block
BlockBreakProperties block_break_pickaxe(float hardness, ToolLevel min_level);  // Requires pickaxe
BlockBreakProperties block_break_axe(float hardness);      // Requires axe
BlockBreakProperties block_break_shovel(float hardness);   // Requires shovel
BlockBreakProperties block_break_instant(void);            // Instant break
BlockBreakProperties block_break_unbreakable(void);        // Unbreakable

// Calculate block break time
float block_calculate_break_time(BlockType type, ToolType tool, ToolLevel level);

// Get break texture (stages 0-9)
TextureUV block_get_break_texture_uv(int stage);

// Get block properties by type
BlockProperties* block_registry_get(BlockType type);

// Get UV coordinates for a specific block face
TextureUV block_registry_get_texture_uv(BlockType type, int face_index);

// Release registry resources
void block_registry_destroy(void);

// Helper function to create textures with the same texture for all sides
BlockTextures block_textures_all(int texture_index);

// Helper function to create textures with different top/bottom/side
BlockTextures block_textures_top_bottom_side(int top, int bottom, int side);

// Helper function to create textures with all individual sides
BlockTextures block_textures_individual(int top, int bottom, int north, int south, int east, int west);

#endif // BLOCK_REGISTRY_H
