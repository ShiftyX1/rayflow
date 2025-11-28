#include "player.h"
#include "../voxel/voxel.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Helper function to get block at world position
static Voxel get_block_at_position(World* world, int x, int y, int z) {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
    
    int chunk_x = (int)floorf((float)x / CHUNK_WIDTH);
    int chunk_z = (int)floorf((float)z / CHUNK_DEPTH);
    
    Chunk* chunk = world_get_chunk(world, chunk_x, chunk_z);
    if (!chunk || !chunk->is_generated) return 0;
    
    int local_x = x - (chunk_x * CHUNK_WIDTH);
    int local_z = z - (chunk_z * CHUNK_DEPTH);
    
    if (local_x < 0) local_x += CHUNK_WIDTH;
    if (local_z < 0) local_z += CHUNK_DEPTH;
    
    if (local_x < 0 || local_x >= CHUNK_WIDTH || local_z < 0 || local_z >= CHUNK_DEPTH) {
        return 0;
    }
    
    return voxel_get(chunk, local_x, y, local_z);
}

Player* player_create(Vector3 spawn_position) {
    Player* player = (Player*)malloc(sizeof(Player));
    if (!player) return NULL;
    
    player->position = spawn_position;
    player->velocity = (Vector3){0.0f, 0.0f, 0.0f};
    
    // Initialize camera - camera IS the player's view, not following it
    player->camera.position = (Vector3){
        spawn_position.x,
        spawn_position.y + PLAYER_EYE_HEIGHT,
        spawn_position.z
    };
    player->camera.target = (Vector3){
        spawn_position.x,
        spawn_position.y + PLAYER_EYE_HEIGHT,
        spawn_position.z - 1.0f
    };
    player->camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    player->camera.fovy = 90.0f;
    player->camera.projection = CAMERA_PERSPECTIVE;
    
    player->yaw = -90.0f;
    player->pitch = 0.0f;
    player->camera_sensitivity = 0.015f;
    
    player->on_ground = false;
    player->is_sprinting = false;
    player->in_creative_mode = false;
    
    player->height = PLAYER_HEIGHT;
    player->width = PLAYER_WIDTH;
    player->eye_height = PLAYER_EYE_HEIGHT;
    
    return player;
}

void player_destroy(Player* player) {
    if (player) {
        free(player);
    }
}

void player_handle_input(Player* player, float delta_time) {
    // Mouse look
    Vector2 mouse_delta = GetMouseDelta();
    player->yaw += mouse_delta.x * player->camera_sensitivity;
    player->pitch -= mouse_delta.y * player->camera_sensitivity;
    
    // Clamp pitch
    if (player->pitch > 89.0f) player->pitch = 89.0f;
    if (player->pitch < -89.0f) player->pitch = -89.0f;
    
    // Calculate camera direction
    Vector3 direction;
    direction.x = cosf(player->yaw * DEG2RAD) * cosf(player->pitch * DEG2RAD);
    direction.y = sinf(player->pitch * DEG2RAD);
    direction.z = sinf(player->yaw * DEG2RAD) * cosf(player->pitch * DEG2RAD);
    
    // Normalize direction
    float dir_len = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    direction.x /= dir_len;
    direction.y /= dir_len;
    direction.z /= dir_len;
    
    // Calculate forward vector (horizontal movement)
    Vector3 forward = direction;
    forward.y = 0;
    float fwd_len = sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (fwd_len > 0.001f) {
        forward.x /= fwd_len;
        forward.z /= fwd_len;
    }
    
    // Calculate right vector
    Vector3 right;
    right.x = forward.z;
    right.y = 0;
    right.z = -forward.x;
    
    // Check sprint
    player->is_sprinting = IsKeyDown(KEY_LEFT_CONTROL);
    float move_speed = player->is_sprinting ? PLAYER_SPRINT_SPEED : PLAYER_SPEED;
    
    // Movement input
    Vector3 move_direction = {0.0f, 0.0f, 0.0f};
    
    if (IsKeyDown(KEY_W)) {
        move_direction.x += forward.x;
        move_direction.z += forward.z;
    }
    if (IsKeyDown(KEY_S)) {
        move_direction.x -= forward.x;
        move_direction.z -= forward.z;
    }
    if (IsKeyDown(KEY_A)) {
        move_direction.x += right.x;
        move_direction.z += right.z;
    }
    if (IsKeyDown(KEY_D)) {
        move_direction.x -= right.x;
        move_direction.z -= right.z;
    }
    
    // Normalize move direction
    float move_len = sqrtf(move_direction.x * move_direction.x + move_direction.z * move_direction.z);
    if (move_len > 0.001f) {
        move_direction.x /= move_len;
        move_direction.z /= move_len;
        
        player->velocity.x = move_direction.x * move_speed;
        player->velocity.z = move_direction.z * move_speed;
    } else {
        player->velocity.x = 0.0f;
        player->velocity.z = 0.0f;
    }
    
    // Creative mode controls
    if (player->in_creative_mode) {
        if (IsKeyDown(KEY_SPACE)) {
            player->velocity.y = move_speed;
        } else if (IsKeyDown(KEY_LEFT_SHIFT)) {
            player->velocity.y = -move_speed;
        } else {
            player->velocity.y = 0.0f;
        }
    } else {
        // Jump
        if (IsKeyDown(KEY_SPACE) && player->on_ground) {
            player->velocity.y = PLAYER_JUMP_VELOCITY;
            player->on_ground = false;
        }
    }
    
    // Toggle creative mode
    if (IsKeyPressed(KEY_C)) {
        player_toggle_creative_mode(player);
        printf("Creative mode: %s\n", player->in_creative_mode ? "ON" : "OFF");
    }
}

void player_apply_gravity(Player* player, float delta_time) {
    if (!player->in_creative_mode) {
        player->velocity.y -= PLAYER_GRAVITY * delta_time;
        
        // Terminal velocity
        if (player->velocity.y < -50.0f) {
            player->velocity.y = -50.0f;
        }
    }
}

bool player_check_collision(Player* player, World* world, Vector3 new_position) {
    // Check collision with voxels around player
    float radius = player->width / 2.0f;
    float height = player->height;
    
    // Check multiple points around player
    Vector3 check_points[] = {
        // Bottom corners
        {new_position.x - radius, new_position.y, new_position.z - radius},
        {new_position.x + radius, new_position.y, new_position.z - radius},
        {new_position.x - radius, new_position.y, new_position.z + radius},
        {new_position.x + radius, new_position.y, new_position.z + radius},
        // Middle
        {new_position.x, new_position.y + height / 2.0f, new_position.z},
        // Top corners
        {new_position.x - radius, new_position.y + height, new_position.z - radius},
        {new_position.x + radius, new_position.y + height, new_position.z - radius},
        {new_position.x - radius, new_position.y + height, new_position.z + radius},
        {new_position.x + radius, new_position.y + height, new_position.z + radius},
    };
    
    for (int i = 0; i < 9; i++) {
        int x = (int)floorf(check_points[i].x);
        int y = (int)floorf(check_points[i].y);
        int z = (int)floorf(check_points[i].z);
        
        Voxel block = get_block_at_position(world, x, y, z);
        if (voxel_is_solid(block)) {
            return true;
        }
    }
    
    return false;
}

void player_resolve_collisions(Player* player, World* world) {
    // Apply velocity and check collisions per axis
    Vector3 new_position;
    
    // X axis
    new_position = player->position;
    new_position.x += player->velocity.x * GetFrameTime();
    if (player_check_collision(player, world, new_position)) {
        player->velocity.x = 0.0f;
    } else {
        player->position.x = new_position.x;
        player->camera.position.x = new_position.x;
    }
    
    // Y axis
    new_position = player->position;
    new_position.y += player->velocity.y * GetFrameTime();
    if (player_check_collision(player, world, new_position)) {
        if (player->velocity.y < 0.0f) {
            player->on_ground = true;
        }
        player->velocity.y = 0.0f;
    } else {
        player->position.y = new_position.y;
        player->camera.position.y = new_position.y + player->eye_height;
        player->on_ground = false;
    }
    
    // Z axis
    new_position = player->position;
    new_position.z += player->velocity.z * GetFrameTime();
    if (player_check_collision(player, world, new_position)) {
        player->velocity.z = 0.0f;
    } else {
        player->position.z = new_position.z;
        player->camera.position.z = new_position.z;
    }
}

void player_update(Player* player, World* world, float delta_time) {
    // Handle input
    player_handle_input(player, delta_time);
    
    // Apply physics
    player_apply_gravity(player, delta_time);
    
    // Resolve collisions and update position
    player_resolve_collisions(player, world);
    
    // Update camera target based on yaw/pitch AFTER position is finalized
    Vector3 direction;
    direction.x = cosf(player->yaw * DEG2RAD) * cosf(player->pitch * DEG2RAD);
    direction.y = sinf(player->pitch * DEG2RAD);
    direction.z = sinf(player->yaw * DEG2RAD) * cosf(player->pitch * DEG2RAD);
    
    player->camera.target.x = player->camera.position.x + direction.x;
    player->camera.target.y = player->camera.position.y + direction.y;
    player->camera.target.z = player->camera.position.z + direction.z;
}

Camera3D player_get_camera(Player* player) {
    return player->camera;
}

void player_toggle_creative_mode(Player* player) {
    player->in_creative_mode = !player->in_creative_mode;
    if (player->in_creative_mode) {
        player->velocity.y = 0.0f;
    }
}
