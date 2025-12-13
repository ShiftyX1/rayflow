#pragma once

#include "world.hpp"
#include "../ecs/components.hpp"
#include <raylib.h>

namespace voxel {

// Result of a block raycast
struct BlockRaycastResult {
    bool hit{false};
    int block_x{0};
    int block_y{0};
    int block_z{0};
    int face{0};  // Which face was hit (0-5)
    float distance{0.0f};
    BlockType block_type{BlockType::Air};
};

class BlockInteraction {
public:
    static constexpr float MAX_REACH_DISTANCE = 5.0f;
    
    void update(World& world, const Vector3& camera_pos, const Vector3& camera_dir, 
                const ecs::ToolHolder& tool, bool is_breaking, float delta_time);
    
    void render_highlight(const Camera3D& camera) const;
    void render_break_overlay(const Camera3D& camera) const;
    static void render_crosshair(int screen_width, int screen_height);
    
    const BlockRaycastResult& get_target() const { return target_; }
    float get_break_progress() const { return break_progress_; }
    
private:
    BlockRaycastResult raycast(const World& world, const Vector3& origin, 
                                const Vector3& direction, float max_distance) const;
    float calculate_break_time(BlockType block_type, const ecs::ToolHolder& tool) const;
    
    BlockRaycastResult target_;
    float break_progress_{0.0f};
    bool was_breaking_{false};
};

} // namespace voxel
