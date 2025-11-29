#ifndef BLOCK_INTERACTION_H
#define BLOCK_INTERACTION_H

#include <raylib.h>
#include <stdbool.h>
#include "voxel.h"
#include "world.h"
#include "block_registry.h"

// Maximum distance for block interaction
#define BLOCK_REACH_DISTANCE 5.0f

// Number of break stages
#define BREAK_STAGES 10

// Raycast result
typedef struct {
    bool hit;                   // Did it hit a block?
    Vector3 block_position;     // Block position (integer coordinates)
    Vector3 hit_position;       // Exact hit position
    Vector3 hit_normal;         // Hit face normal
    int block_x, block_y, block_z; // Block coordinates
    BlockType block_type;       // Block type
    float distance;             // Distance to the block
} BlockRaycastResult;

// Block break state
typedef struct {
    bool is_breaking;           // Is breaking
    int target_x, target_y, target_z; // Coordinates of the block being broken
    BlockType target_type;      // Type of the block being broken
    float break_progress;       // Break progress (0.0 - 1.0)
    float total_break_time;     // Total break time
    int current_stage;          // Current stage (0-9)
} BlockBreakState;

// Block interaction system
typedef struct {
    BlockBreakState break_state;
    BlockRaycastResult current_target;
    ToolType current_tool;
    ToolLevel current_tool_level;
} BlockInteraction;

// Initialize the block interaction system
void block_interaction_init(BlockInteraction* interaction);

// Raycast to find a block (DDA algorithm)
BlockRaycastResult block_raycast(World* world, Vector3 origin, Vector3 direction, float max_distance);

// Update the interaction system (call every frame)
void block_interaction_update(BlockInteraction* interaction, World* world, 
                               Vector3 camera_position, Vector3 camera_direction, 
                               bool is_breaking, float delta_time);

// Start breaking a block
void block_interaction_start_break(BlockInteraction* interaction, World* world, 
                                    int x, int y, int z, BlockType type);

// Stop breaking a block
void block_interaction_stop_break(BlockInteraction* interaction);

// Set the current tool
void block_interaction_set_tool(BlockInteraction* interaction, ToolType tool, ToolLevel level);

// Get the break stage (0-9, -1 if not breaking)
int block_interaction_get_break_stage(BlockInteraction* interaction);

// Render block highlight (outline)
void block_interaction_render_highlight(BlockInteraction* interaction, Camera3D camera);

// Render break overlay
void block_interaction_render_break_overlay(BlockInteraction* interaction, Camera3D camera);

// Render crosshair in the center of the screen
void block_interaction_render_crosshair(int screen_width, int screen_height);

#endif // BLOCK_INTERACTION_H
