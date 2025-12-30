#pragma once

#include "world.hpp"
#include "../ecs/components.hpp"
#include "../../shared/constants.hpp"
#include <raylib.h>

#include <optional>
#include <array>

namespace voxel {

struct BlockRaycastResult {
    bool hit{false};
    int block_x{0};
    int block_y{0};
    int block_z{0};
    int face{0};
    float distance{0.0f};
    float hitY{0.5f};
    BlockType block_type{BlockType::Air};
};

class BlockInteraction {
public:
    static constexpr float MAX_REACH_DISTANCE = shared::kBlockReachDistance;
    static constexpr int DESTROY_STAGE_COUNT = 10;

    struct BreakRequest {
        int x{0};
        int y{0};
        int z{0};
    };

    struct PlaceRequest {
        int x{0};
        int y{0};
        int z{0};
        BlockType block_type{BlockType::Air};
        float hitY{0.5f};
        std::uint8_t face{0};
    };

    bool init();
    void destroy();
    
    void update(World& world, const Vector3& camera_pos, const Vector3& camera_dir, 
                const ecs::ToolHolder& tool, bool is_breaking, bool is_placing, float delta_time);
    
    void render_highlight(const Camera3D& camera) const;
    void render_break_overlay(const Camera3D& camera) const;
    static void render_crosshair(int screen_width, int screen_height);
    
    const BlockRaycastResult& get_target() const { return target_; }
    float get_break_progress() const { return break_progress_; }

    std::optional<BreakRequest> consume_break_request();
    std::optional<PlaceRequest> consume_place_request();

    void on_action_rejected();
    
private:
    BlockRaycastResult raycast(const World& world, const Vector3& origin, 
                                const Vector3& direction, float max_distance) const;
    float calculate_break_time(BlockType block_type, const ecs::ToolHolder& tool) const;
    
    BlockRaycastResult target_;
    float break_progress_{0.0f};
    bool was_breaking_{false};

    bool was_placing_{false};

    std::optional<BreakRequest> pending_break_;
    std::optional<PlaceRequest> pending_place_;

    std::optional<BreakRequest> outgoing_break_;
    std::optional<PlaceRequest> outgoing_place_;

    std::array<Texture2D, DESTROY_STAGE_COUNT> destroy_textures_{};
    bool textures_loaded_{false};
};

} // namespace voxel
