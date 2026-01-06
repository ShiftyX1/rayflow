#pragma once

// =============================================================================
// Rendering ECS Components - Visual components (requires raylib)
// =============================================================================
//
// These components require raylib and are part of engine_client.
// For headless/server code, use components/common.hpp instead.
//

#include <raylib.h>
#include <cstdint>

namespace ecs {

// =============================================================================
// Sprites
// =============================================================================

/// Static sprite component
struct Sprite {
    Texture2D texture{};
    Rectangle source{0, 0, 0, 0};  // source rect in texture (0,0,0,0 = full texture)
    Vector2 origin{0, 0};          // pivot point (0,0 = top-left)
    Color tint{WHITE};
    float scale{1.0f};
    int z_order{0};                // draw order (higher = on top)
    bool flip_x{false};
    bool flip_y{false};
};

/// Animated sprite component
struct AnimatedSprite {
    Texture2D spritesheet{};
    int frame_width{0};            // width of single frame
    int frame_height{0};           // height of single frame
    int frame{0};                  // current frame
    int frame_count{1};            // total frames in animation
    int frames_per_row{1};         // frames per row in spritesheet
    float frame_time{0.1f};        // seconds per frame
    float timer{0.0f};             // current timer
    bool loop{true};
    bool playing{true};
    Vector2 origin{0, 0};
    Color tint{WHITE};
    float scale{1.0f};
    int z_order{0};
    bool flip_x{false};
    bool flip_y{false};
};

/// Multiple animations stored by name/id
struct AnimationSet {
    static constexpr int kMaxAnimations = 16;
    
    struct Animation {
        int start_frame{0};
        int frame_count{1};
        float frame_time{0.1f};
        bool loop{true};
    };
    
    Animation animations[kMaxAnimations]{};
    int animation_count{0};
    int current_animation{0};
};

// =============================================================================
// Camera 2D
// =============================================================================

/// 2D camera controller component
struct Camera2DController {
    Vector2 offset{0, 0};          // camera offset from target
    float zoom{1.0f};
    float rotation{0.0f};          // degrees
    float smoothing{5.0f};         // interpolation speed (0 = instant)
    
    // Screen shake
    float shake_intensity{0.0f};   // current shake amount (pixels)
    float shake_decay{5.0f};       // how fast shake diminishes
    float shake_frequency{30.0f};  // shake oscillation speed
    float shake_timer{0.0f};
};

/// Camera bounds (limits where camera can go)
struct CameraBounds {
    float min_x{0.0f};
    float min_y{0.0f};
    float max_x{1000.0f};
    float max_y{1000.0f};
    bool enabled{false};
};

/// Camera follow target
struct CameraTarget {
    std::uint32_t entity_id{0};    // entity to follow
    bool has_target{false};
    Vector2 look_ahead{0, 0};      // offset in direction of movement
    float look_ahead_factor{0.0f}; // how much to look ahead (0-1)
};

// =============================================================================
// Particles
// =============================================================================

/// Single particle data
struct Particle {
    float x{0}, y{0};
    float vx{0}, vy{0};
    float life{1.0f};
    float max_life{1.0f};
    float size{4.0f};
    float rotation{0.0f};
    float angular_velocity{0.0f};
    Color color{WHITE};
    Color end_color{WHITE};
    bool active{false};
};

/// Particle emitter component
struct ParticleEmitter {
    static constexpr int kMaxParticles = 256;
    
    Particle particles[kMaxParticles]{};
    int active_count{0};
    
    // Emission settings
    float emit_rate{10.0f};        // particles per second
    float emit_timer{0.0f};
    bool emitting{true};
    bool one_shot{false};          // emit all at once then stop
    int burst_count{10};           // particles per burst (for one_shot)
    
    // Particle settings
    float lifetime_min{0.5f};
    float lifetime_max{1.0f};
    float speed_min{50.0f};
    float speed_max{100.0f};
    float direction{0.0f};         // radians, 0 = right
    float spread{3.14159f};        // radians, PI = full circle
    float size_min{2.0f};
    float size_max{8.0f};
    float size_end{0.0f};          // size at end of life
    float gravity{0.0f};           // downward acceleration
    Color color_start{WHITE};
    Color color_end{WHITE};
    
    // Offset from entity position
    float offset_x{0.0f};
    float offset_y{0.0f};
};

// =============================================================================
// Visual Effects
// =============================================================================

/// Flash effect (e.g., damage flash)
struct FlashEffect {
    Color color{WHITE};
    float duration{0.1f};
    float timer{0.0f};
    bool active{false};
};

/// Trail effect
struct TrailEffect {
    static constexpr int kMaxPoints = 32;
    
    Vector2 points[kMaxPoints]{};
    int point_count{0};
    int head{0};
    float point_interval{0.02f};   // seconds between points
    float timer{0.0f};
    float width_start{8.0f};
    float width_end{1.0f};
    Color color_start{WHITE};
    Color color_end{WHITE};
    bool enabled{true};
};

// =============================================================================
// UI Elements (in-world)
// =============================================================================

/// Health bar rendered above entity
struct HealthBar {
    float width{32.0f};
    float height{4.0f};
    float offset_y{-20.0f};        // offset above entity
    Color background{DARKGRAY};
    Color foreground{RED};
    Color border{BLACK};
    bool visible{true};
    bool show_when_full{false};    // hide when health is full
};

/// Text label above entity
struct WorldLabel {
    const char* text{nullptr};
    float offset_y{-30.0f};
    int font_size{10};
    Color color{WHITE};
    bool visible{true};
};

// =============================================================================
// Render Layers
// =============================================================================

/// Render layer for ordering
struct RenderLayer {
    enum Layer : std::uint8_t {
        Background = 0,
        Ground = 10,
        Shadows = 20,
        Entities = 50,
        Player = 60,
        Effects = 70,
        Projectiles = 80,
        Particles = 90,
        UI = 100
    };
    
    std::uint8_t layer{Entities};
};

} // namespace ecs

