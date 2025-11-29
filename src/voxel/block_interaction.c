#include "block_interaction.h"
#include "block_registry.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>

void block_interaction_init(BlockInteraction* interaction) {
    if (!interaction) return;
    
    interaction->break_state.is_breaking = false;
    interaction->break_state.break_progress = 0.0f;
    interaction->break_state.total_break_time = 0.0f;
    interaction->break_state.current_stage = -1;
    
    interaction->current_target.hit = false;
    interaction->current_tool = TOOL_NONE;
    interaction->current_tool_level = TOOL_LEVEL_HAND;
}

static Voxel get_world_block(World* world, int x, int y, int z) {
    if (y < 0 || y >= CHUNK_HEIGHT) return BLOCK_AIR;
    
    int chunk_x = (int)floorf((float)x / CHUNK_WIDTH);
    int chunk_z = (int)floorf((float)z / CHUNK_DEPTH);
    
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_z);
    if (!chunk || !chunk->is_generated) return BLOCK_AIR;
    
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_z = z - (chunk_z * CHUNK_DEPTH);
    
    if (local_x < 0) local_x += CHUNK_WIDTH;
    if (local_z < 0) local_z += CHUNK_DEPTH;
    
    return voxel_get(chunk, local_x, y, local_z);
}

static void set_world_block(World* world, int x, int y, int z, Voxel type) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;
    
    int chunk_x = (int)floorf((float)x / CHUNK_WIDTH);
    int chunk_z = (int)floorf((float)z / CHUNK_DEPTH);
    
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_z);
    if (!chunk || !chunk->is_generated) return;
    
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_z = z - (chunk_z * CHUNK_DEPTH);
    
    if (local_x < 0) local_x += CHUNK_WIDTH;
    if (local_z < 0) local_z += CHUNK_DEPTH;
    
    voxel_set(chunk, local_x, y, local_z, type);
    chunk->needs_mesh_update = true;
}

BlockRaycastResult block_raycast(World* world, Vector3 origin, Vector3 direction, float max_distance) {
    BlockRaycastResult result = {0};
    result.hit = false;
    
    float dir_len = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (dir_len < 0.0001f) return result;
    
    Vector3 dir = {
        direction.x / dir_len,
        direction.y / dir_len,
        direction.z / dir_len
    };
    
    int x = (int)floorf(origin.x);
    int y = (int)floorf(origin.y);
    int z = (int)floorf(origin.z);
    
    int step_x = (dir.x > 0) ? 1 : -1;
    int step_y = (dir.y > 0) ? 1 : -1;
    int step_z = (dir.z > 0) ? 1 : -1;
    
    float t_delta_x = (fabsf(dir.x) > 0.0001f) ? fabsf(1.0f / dir.x) : 1e10f;
    float t_delta_y = (fabsf(dir.y) > 0.0001f) ? fabsf(1.0f / dir.y) : 1e10f;
    float t_delta_z = (fabsf(dir.z) > 0.0001f) ? fabsf(1.0f / dir.z) : 1e10f;
    
    float t_max_x, t_max_y, t_max_z;
    
    if (dir.x > 0) {
        t_max_x = ((float)(x + 1) - origin.x) / dir.x;
    } else {
        t_max_x = (origin.x - (float)x) / fabsf(dir.x);
    }
    
    if (dir.y > 0) {
        t_max_y = ((float)(y + 1) - origin.y) / dir.y;
    } else {
        t_max_y = (origin.y - (float)y) / fabsf(dir.y);
    }
    
    if (dir.z > 0) {
        t_max_z = ((float)(z + 1) - origin.z) / dir.z;
    } else {
        t_max_z = (origin.z - (float)z) / fabsf(dir.z);
    }
    
    float distance = 0.0f;
    int last_axis = -1; // 0=x, 1=y, 2=z
    
    // DDA loop
    while (distance < max_distance) {
        Voxel block = get_world_block(world, x, y, z);
        if (block != BLOCK_AIR && voxel_is_solid(block)) {
            result.hit = true;
            result.block_x = x;
            result.block_y = y;
            result.block_z = z;
            result.block_position = (Vector3){(float)x, (float)y, (float)z};
            result.block_type = (BlockType)block;
            result.distance = distance;
            
            result.hit_position = (Vector3){
                origin.x + dir.x * distance,
                origin.y + dir.y * distance,
                origin.z + dir.z * distance
            };
            
            result.hit_normal = (Vector3){0, 0, 0};
            switch (last_axis) {
                case 0: result.hit_normal.x = -step_x; break;
                case 1: result.hit_normal.y = -step_y; break;
                case 2: result.hit_normal.z = -step_z; break;
                default:
                    {
                        float fx = origin.x - (float)x;
                        float fy = origin.y - (float)y;
                        float fz = origin.z - (float)z;
                        
                        float min_dist = fx;
                        result.hit_normal = (Vector3){-1, 0, 0};
                        
                        if (1.0f - fx < min_dist) { min_dist = 1.0f - fx; result.hit_normal = (Vector3){1, 0, 0}; }
                        if (fy < min_dist) { min_dist = fy; result.hit_normal = (Vector3){0, -1, 0}; }
                        if (1.0f - fy < min_dist) { min_dist = 1.0f - fy; result.hit_normal = (Vector3){0, 1, 0}; }
                        if (fz < min_dist) { min_dist = fz; result.hit_normal = (Vector3){0, 0, -1}; }
                        if (1.0f - fz < min_dist) { result.hit_normal = (Vector3){0, 0, 1}; }
                    }
                    break;
            }
            
            return result;
        }
        
        if (t_max_x < t_max_y && t_max_x < t_max_z) {
            x += step_x;
            distance = t_max_x;
            t_max_x += t_delta_x;
            last_axis = 0;
        } else if (t_max_y < t_max_z) {
            y += step_y;
            distance = t_max_y;
            t_max_y += t_delta_y;
            last_axis = 1;
        } else {
            z += step_z;
            distance = t_max_z;
            t_max_z += t_delta_z;
            last_axis = 2;
        }
    }
    
    return result;
}

void block_interaction_start_break(BlockInteraction* interaction, World* world, 
                                    int x, int y, int z, BlockType type) {
    if (!interaction) return;
    
    interaction->break_state.is_breaking = true;
    interaction->break_state.target_x = x;
    interaction->break_state.target_y = y;
    interaction->break_state.target_z = z;
    interaction->break_state.target_type = type;
    interaction->break_state.break_progress = 0.0f;
    
    interaction->break_state.total_break_time = block_calculate_break_time(
        type, interaction->current_tool, interaction->current_tool_level);
    
    interaction->break_state.current_stage = 0;
}

void block_interaction_stop_break(BlockInteraction* interaction) {
    if (!interaction) return;
    
    interaction->break_state.is_breaking = false;
    interaction->break_state.break_progress = 0.0f;
    interaction->break_state.current_stage = -1;
}

void block_interaction_set_tool(BlockInteraction* interaction, ToolType tool, ToolLevel level) {
    if (!interaction) return;
    
    interaction->current_tool = tool;
    interaction->current_tool_level = level;
}

int block_interaction_get_break_stage(BlockInteraction* interaction) {
    if (!interaction || !interaction->break_state.is_breaking) return -1;
    return interaction->break_state.current_stage;
}

void block_interaction_update(BlockInteraction* interaction, World* world, 
                               Vector3 camera_position, Vector3 camera_direction, 
                               bool is_breaking, float delta_time) {
    if (!interaction || !world) return;
    
    interaction->current_target = block_raycast(world, camera_position, camera_direction, BLOCK_REACH_DISTANCE);
    
    if (is_breaking && interaction->current_target.hit) {
        BlockRaycastResult* target = &interaction->current_target;
        BlockBreakState* state = &interaction->break_state;
        
        if (!state->is_breaking || 
            state->target_x != target->block_x || 
            state->target_y != target->block_y || 
            state->target_z != target->block_z) {
            block_interaction_start_break(interaction, world, 
                target->block_x, target->block_y, target->block_z, 
                target->block_type);
        }
        
        if (state->is_breaking && state->total_break_time >= 0.0f) {
            if (state->total_break_time == 0.0f) {
                set_world_block(world, state->target_x, state->target_y, state->target_z, BLOCK_AIR);
                block_interaction_stop_break(interaction);
            } else {
                state->break_progress += delta_time;
                
                float progress_ratio = state->break_progress / state->total_break_time;
                state->current_stage = (int)(progress_ratio * BREAK_STAGES);
                if (state->current_stage >= BREAK_STAGES) state->current_stage = BREAK_STAGES - 1;
                
                if (state->break_progress >= state->total_break_time) {
                    set_world_block(world, state->target_x, state->target_y, state->target_z, BLOCK_AIR);
                    block_interaction_stop_break(interaction);
                }
            }
        }
    } else if (!is_breaking || !interaction->current_target.hit) {
        if (interaction->break_state.is_breaking) {
            block_interaction_stop_break(interaction);
        }
    }
}

// Rendering block highlight (thin outline)
void block_interaction_render_highlight(BlockInteraction* interaction, Camera3D camera) {
    if (!interaction || !interaction->current_target.hit) return;
    
    BlockRaycastResult* target = &interaction->current_target;
    
    float offset = 0.002f;
    float x0 = (float)target->block_x - offset;
    float y0 = (float)target->block_y - offset;
    float z0 = (float)target->block_z - offset;
    float x1 = (float)target->block_x + 1.0f + offset;
    float y1 = (float)target->block_y + 1.0f + offset;
    float z1 = (float)target->block_z + 1.0f + offset;
    
    Color highlight_color = (Color){0, 0, 0, 100};
    
    rlDisableDepthMask();
    
    rlBegin(RL_LINES);
        rlColor4ub(highlight_color.r, highlight_color.g, highlight_color.b, highlight_color.a);
        
        rlVertex3f(x0, y0, z0); rlVertex3f(x1, y0, z0);
        rlVertex3f(x1, y0, z0); rlVertex3f(x1, y0, z1);
        rlVertex3f(x1, y0, z1); rlVertex3f(x0, y0, z1);
        rlVertex3f(x0, y0, z1); rlVertex3f(x0, y0, z0);
        
        rlVertex3f(x0, y1, z0); rlVertex3f(x1, y1, z0);
        rlVertex3f(x1, y1, z0); rlVertex3f(x1, y1, z1);
        rlVertex3f(x1, y1, z1); rlVertex3f(x0, y1, z1);
        rlVertex3f(x0, y1, z1); rlVertex3f(x0, y1, z0);
        
        rlVertex3f(x0, y0, z0); rlVertex3f(x0, y1, z0);
        rlVertex3f(x1, y0, z0); rlVertex3f(x1, y1, z0);
        rlVertex3f(x1, y0, z1); rlVertex3f(x1, y1, z1);
        rlVertex3f(x0, y0, z1); rlVertex3f(x0, y1, z1);
    rlEnd();
    
    rlEnableDepthMask();
}

void block_interaction_render_break_overlay(BlockInteraction* interaction, Camera3D camera) {
    // TODO: Implement break overlay rendering
    (void)interaction;
    (void)camera;
}

void block_interaction_render_crosshair(int screen_width, int screen_height) {
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;
    
    // Crosshair parameters
    int size = 10;
    int thickness = 2;
    int gap = 3; // Gap in the center
    
    Color crosshair_color = (Color){255, 255, 255, 200};
    Color shadow_color = (Color){0, 0, 0, 100};
    
    // Shadow for contrast
    // Horizontal line (left part)
    DrawRectangle(center_x - size - 1, center_y - thickness/2 + 1, size - gap, thickness, shadow_color);
    // Horizontal line (right part)
    DrawRectangle(center_x + gap + 1, center_y - thickness/2 + 1, size - gap, thickness, shadow_color);
    // Vertical line (top part)
    DrawRectangle(center_x - thickness/2 + 1, center_y - size + 1, thickness, size - gap, shadow_color);
    // Vertical line (bottom part)
    DrawRectangle(center_x - thickness/2 + 1, center_y + gap + 1, thickness, size - gap, shadow_color);
    
    // Main crosshair
    // Horizontal line (left part)
    DrawRectangle(center_x - size, center_y - thickness/2, size - gap, thickness, crosshair_color);
    // Horizontal line (right part)
    DrawRectangle(center_x + gap, center_y - thickness/2, size - gap, thickness, crosshair_color);
    // Vertical line (top part)
    DrawRectangle(center_x - thickness/2, center_y - size, thickness, size - gap, crosshair_color);
    // Vertical line (bottom part)
    DrawRectangle(center_x - thickness/2, center_y + gap, thickness, size - gap, crosshair_color);
}
