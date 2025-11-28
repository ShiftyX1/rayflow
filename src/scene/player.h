#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>
#include <stdbool.h>
#include "../voxel/world.h"

// Player physics constants
#define PLAYER_SPEED 5.0f
#define PLAYER_SPRINT_SPEED 8.0f
#define PLAYER_JUMP_VELOCITY 8.0f
#define PLAYER_GRAVITY 20.0f
#define PLAYER_HEIGHT 1.8f
#define PLAYER_WIDTH 0.6f
#define PLAYER_EYE_HEIGHT 1.62f

// Player structure
typedef struct {
    // Position and physics
    Vector3 position;          // Player position (feet level)
    Vector3 velocity;          // Current velocity
    
    // Camera
    Camera3D camera;           // First-person camera
    Vector3 camera_smooth_pos; // Smoothed camera position
    float yaw;                 // Horizontal rotation
    float pitch;               // Vertical rotation
    float camera_sensitivity;  // Mouse sensitivity
    
    // State
    bool on_ground;            // Is player standing on ground
    bool is_sprinting;         // Is player sprinting
    bool in_creative_mode;     // Creative mode (no gravity, fly)
    
    // Dimensions
    float height;              // Player height
    float width;               // Player width (radius)
    float eye_height;          // Camera height offset from feet
} Player;

// Player functions
Player* player_create(Vector3 spawn_position);
void player_destroy(Player* player);
void player_update(Player* player, World* world, float delta_time);
void player_handle_input(Player* player, float delta_time);
Camera3D player_get_camera(Player* player);
void player_toggle_creative_mode(Player* player);

// Internal physics functions
void player_apply_gravity(Player* player, float delta_time);
void player_apply_movement(Player* player, float delta_time);
bool player_check_collision(Player* player, World* world, Vector3 new_position);
void player_resolve_collisions(Player* player, World* world);

#endif // PLAYER_H
