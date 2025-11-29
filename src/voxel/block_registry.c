#include "block_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global block registry
BlockRegistry g_block_registry = {0};

// Constants for break speed calculation
#define BASE_BREAK_SPEED 1.0f
#define TOOL_SPEED_WOOD 2.0f
#define TOOL_SPEED_STONE 4.0f
#define TOOL_SPEED_IRON 6.0f
#define TOOL_SPEED_DIAMOND 8.0f
#define TOOL_SPEED_NETHERITE 9.0f
#define TOOL_SPEED_GOLD 12.0f  // Gold is faster but less durable

// Break texture indices in the atlas (240-249)
#define BREAK_TEXTURE_START 240
#define BREAK_TEXTURE_STAGES 10

// ================== Functions for break properties ==================

// Default block without tool requirements
BlockBreakProperties block_break_default(float hardness) {
    return (BlockBreakProperties){
        .hardness = hardness,
        .required_tool = TOOL_NONE,
        .required_level = TOOL_LEVEL_HAND,
        .tool_multiplier = 1.0f,
        .instant_break = false
    };
}

// Block requiring a pickaxe
BlockBreakProperties block_break_pickaxe(float hardness, ToolLevel min_level) {
    return (BlockBreakProperties){
        .hardness = hardness,
        .required_tool = TOOL_PICKAXE,
        .required_level = min_level,
        .tool_multiplier = 5.0f,  // Pickaxe speeds up breaking by 5 times
        .instant_break = false
    };
}

// Block requiring an axe
BlockBreakProperties block_break_axe(float hardness) {
    return (BlockBreakProperties){
        .hardness = hardness,
        .required_tool = TOOL_AXE,
        .required_level = TOOL_LEVEL_HAND,  // Wood can be chopped by hand, axe speeds up
        .tool_multiplier = 5.0f,
        .instant_break = false
    };
}

// Block requiring a shovel
BlockBreakProperties block_break_shovel(float hardness) {
    return (BlockBreakProperties){
        .hardness = hardness,
        .required_tool = TOOL_SHOVEL,
        .required_level = TOOL_LEVEL_HAND,
        .tool_multiplier = 5.0f,
        .instant_break = false
    };
}

// Instant break (flowers, grass)
BlockBreakProperties block_break_instant(void) {
    return (BlockBreakProperties){
        .hardness = 0.0f,
        .required_tool = TOOL_NONE,
        .required_level = TOOL_LEVEL_HAND,
        .tool_multiplier = 1.0f,
        .instant_break = true
    };
}

// Unbreakable block (bedrock)
BlockBreakProperties block_break_unbreakable(void) {
    return (BlockBreakProperties){
        .hardness = -1.0f,
        .required_tool = TOOL_NONE,
        .required_level = TOOL_LEVEL_HAND,
        .tool_multiplier = 1.0f,
        .instant_break = false
    };
}

// Get tool speed by level
static float get_tool_speed(ToolLevel level) {
    switch (level) {
        case TOOL_LEVEL_WOOD: return TOOL_SPEED_WOOD;
        case TOOL_LEVEL_STONE: return TOOL_SPEED_STONE;
        case TOOL_LEVEL_IRON: return TOOL_SPEED_IRON;
        case TOOL_LEVEL_DIAMOND: return TOOL_SPEED_DIAMOND;
        case TOOL_LEVEL_NETHERITE: return TOOL_SPEED_NETHERITE;
        default: return BASE_BREAK_SPEED;
    }
}

// Calculate block break time
float block_calculate_break_time(BlockType type, ToolType tool, ToolLevel level) {
    BlockProperties* props = block_registry_get(type);
    if (!props) return -1.0f;
    
    BlockBreakProperties* break_props = &props->break_props;
    
    // Unbreakable
    if (break_props->hardness < 0.0f) return -1.0f;
    
    // Instant break
    if (break_props->instant_break) return 0.0f;
    
    float base_time = break_props->hardness;
    float speed_multiplier = BASE_BREAK_SPEED;
    
    // Check tool compatibility
    bool correct_tool = (tool == break_props->required_tool);
    bool sufficient_level = (level >= break_props->required_level);
    
    if (correct_tool && sufficient_level) {
        // Correct tool - speed up
        speed_multiplier = get_tool_speed(level) * break_props->tool_multiplier;
    } else if (break_props->required_tool != TOOL_NONE && !correct_tool) {
        // Incorrect tool for a block requiring a specific one - slow down
        speed_multiplier = 0.3f;
    }
    
    // Time = base_time * 1.5 / speed_multiplier
    float break_time = (base_time * 1.5f) / speed_multiplier;
    
    return break_time;
}

// Get break texture UV
TextureUV block_get_break_texture_uv(int stage) {
    if (stage < 0) stage = 0;
    if (stage >= BREAK_TEXTURE_STAGES) stage = BREAK_TEXTURE_STAGES - 1;
    
    int texture_index = BREAK_TEXTURE_START + stage;
    
    if (g_block_registry.atlas) {
        return texture_atlas_get_uv(g_block_registry.atlas, texture_index);
    }
    
    // Return empty UV if atlas is not loaded
    TextureUV empty = {0, 0, 0, 0};
    return empty;
}

// ================== Helper functions for textures ==================

// Helper function to create textures with one texture for all sides
BlockTextures block_textures_all(int texture_index) {
    BlockTextures tex = {0};
    tex.top = texture_index;
    tex.bottom = texture_index;
    tex.side = texture_index;
    tex.north = texture_index;
    tex.south = texture_index;
    tex.east = texture_index;
    tex.west = texture_index;
    tex.use_individual_sides = false;
    return tex;
}

// Helper function to create textures with different top/bottom/side
BlockTextures block_textures_top_bottom_side(int top, int bottom, int side) {
    BlockTextures tex = {0};
    tex.top = top;
    tex.bottom = bottom;
    tex.side = side;
    tex.north = side;
    tex.south = side;
    tex.east = side;
    tex.west = side;
    tex.use_individual_sides = false;
    return tex;
}

// Helper function to create textures with all individual sides
BlockTextures block_textures_individual(int top, int bottom, int north, int south, int east, int west) {
    BlockTextures tex = {0};
    tex.top = top;
    tex.bottom = bottom;
    tex.north = north;
    tex.south = south;
    tex.east = east;
    tex.west = west;
    tex.side = north; // Запасное значение
    tex.use_individual_sides = true;
    return tex;
}

// Register block
void block_registry_register(BlockType type, const char* name, 
                             bool is_solid, bool is_transparent, bool is_opaque,
                             bool is_liquid, float light_emission,
                             BlockTextures textures, BlockBreakProperties break_props) {
    if (type >= BLOCK_COUNT) {
        fprintf(stderr, "Error: Invalid block type %d\n", type);
        return;
    }

    BlockProperties* props = &g_block_registry.blocks[type];
    props->type = type;
    props->name = name;
    props->is_solid = is_solid;
    props->is_transparent = is_transparent;
    props->is_opaque = is_opaque;
    props->is_liquid = is_liquid;
    props->light_emission = light_emission;
    props->textures = textures;
    props->break_props = break_props;
}

// Initialize block registry
bool block_registry_init(const char* atlas_path) {
    if (g_block_registry.is_initialized) {
        fprintf(stderr, "Warning: Block registry already initialized\n");
        return true;
    }

    // Load texture atlas
    g_block_registry.atlas = texture_atlas_create(atlas_path);
    if (!g_block_registry.atlas) {
        fprintf(stderr, "Error: Failed to load texture atlas\n");
        return false;
    }

    // Initialize all blocks as air
    memset(g_block_registry.blocks, 0, sizeof(g_block_registry.blocks));

    // Register blocks (approximate atlas layout - will need to be adjusted according to the actual atlas)
    // Indices are calculated as: y * tiles_per_row + x
    
    // Air - invisible block
    block_registry_register(BLOCK_AIR, "Air", false, true, false, false, 0.0f, 
                           block_textures_all(0), block_break_instant());
    
    // Grass - different top, bottom, and sides (hardness: 0.6, shovel speeds up)
    block_registry_register(BLOCK_GRASS, "Grass", true, false, true, false, 0.0f,
                           block_textures_top_bottom_side(0, 2, 3), block_break_shovel(0.6f));
    
    // Dirt - same on all sides (hardness: 0.5)
    block_registry_register(BLOCK_DIRT, "Dirt", true, false, true, false, 0.0f,
                           block_textures_all(2), block_break_shovel(0.5f));
    
    // Stone (hardness: 1.5, requires pickaxe)
    block_registry_register(BLOCK_STONE, "Stone", true, false, true, false, 0.0f,
                           block_textures_all(1), block_break_pickaxe(1.5f, TOOL_LEVEL_WOOD));
    
    // Sand (hardness: 0.5, shovel speeds up)
    block_registry_register(BLOCK_SAND, "Sand", true, false, true, false, 0.0f,
                           block_textures_all(18), block_break_shovel(0.5f));
    
    // Water - transparent (unbreakable)
    block_registry_register(BLOCK_WATER, "Water", false, true, false, true, 0.0f,
                           block_textures_all(5), block_break_unbreakable());
    
    // Wood - different top/bottom and sides (hardness: 2.0, axe speeds up)
    block_registry_register(BLOCK_WOOD, "Wood", true, false, true, false, 0.0f,
                           block_textures_top_bottom_side(6, 6, 7), block_break_axe(2.0f));
    
    // Leaves - semi-transparent (hardness: 0.2, shears or axe speeds up)
    block_registry_register(BLOCK_LEAVES, "Leaves", true, true, false, false, 0.0f,
                           block_textures_all(8), block_break_default(0.2f));
    
    // Cobblestone (hardness: 2.0, requires pickaxe)
    block_registry_register(BLOCK_COBBLESTONE, "Cobblestone", true, false, true, false, 0.0f,
                           block_textures_all(9), block_break_pickaxe(2.0f, TOOL_LEVEL_WOOD));
    
    // Planks (hardness: 2.0, axe speeds up)
    block_registry_register(BLOCK_PLANKS, "Planks", true, false, true, false, 0.0f,
                           block_textures_all(4), block_break_axe(2.0f));
    
    // Glass - transparent (hardness: 0.3, breaks instantly, drops nothing)
    block_registry_register(BLOCK_GLASS, "Glass", true, true, false, false, 0.0f,
                           block_textures_all(11), block_break_default(0.3f));
    
    // Brick (hardness: 2.0, requires pickaxe)
    block_registry_register(BLOCK_BRICK, "Brick", true, false, true, false, 0.0f,
                           block_textures_all(12), block_break_pickaxe(2.0f, TOOL_LEVEL_WOOD));

    g_block_registry.is_initialized = true;
    
    printf("Block registry initialized with %d blocks\n", BLOCK_COUNT);
    return true;
}

// Get block properties by type
BlockProperties* block_registry_get(BlockType type) {
    if (type >= BLOCK_COUNT) {
        return &g_block_registry.blocks[BLOCK_AIR];
    }
    return &g_block_registry.blocks[type];
}

// Get UV coordinates for a specific side of a block
// face_index: 0=top, 1=bottom, 2=north, 3=south, 4=east, 5=west
TextureUV block_registry_get_texture_uv(BlockType type, int face_index) {
    if (!g_block_registry.is_initialized || !g_block_registry.atlas) {
        TextureUV empty = {0};
        return empty;
    }

    BlockProperties* props = block_registry_get(type);
    int texture_index = 0;

    // Choose the appropriate texture based on the side
    if (props->textures.use_individual_sides) {
        switch (face_index) {
            case 0: texture_index = props->textures.top; break;
            case 1: texture_index = props->textures.bottom; break;
            case 2: texture_index = props->textures.north; break;
            case 3: texture_index = props->textures.south; break;
            case 4: texture_index = props->textures.east; break;
            case 5: texture_index = props->textures.west; break;
            default: texture_index = props->textures.side; break;
        }
    } else {
        switch (face_index) {
            case 0: texture_index = props->textures.top; break;
            case 1: texture_index = props->textures.bottom; break;
            default: texture_index = props->textures.side; break;
        }
    }

    return texture_atlas_get_uv(g_block_registry.atlas, texture_index);
}

// Free block registry resources
void block_registry_destroy(void) {
    if (!g_block_registry.is_initialized) {
        return;
    }

    if (g_block_registry.atlas) {
        texture_atlas_destroy(g_block_registry.atlas);
        g_block_registry.atlas = NULL;
    }

    g_block_registry.is_initialized = false;
    printf("Block registry destroyed\n");
}
