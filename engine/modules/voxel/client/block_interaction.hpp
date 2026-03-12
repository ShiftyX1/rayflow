#pragma once

#include "world.hpp"
#include "engine/ecs/components.hpp"
#include "engine/core/player_constants.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/gl_texture.hpp"
#include "engine/renderer/gl_shader.hpp"
#include "engine/renderer/gl_mesh.hpp"
#include "engine/renderer/camera.hpp"

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
    static constexpr float MAX_REACH_DISTANCE = engine::kBlockReachDistance;
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
    
    void update(World& world, const rf::Vec3& camera_pos, const rf::Vec3& camera_dir, 
                const ecs::ToolHolder& tool, bool is_breaking, bool is_placing, float delta_time);
    
    /// Render wireframe outline around the targeted block.
    void render_highlight(const rf::Camera& camera) const;

    /// Render destroy stage texture overlay on the targeted block.
    void render_break_overlay(const rf::Camera& camera) const;

    static void render_crosshair(int screen_width, int screen_height);
    
    const BlockRaycastResult& get_target() const { return target_; }
    float get_break_progress() const { return break_progress_; }

    std::optional<BreakRequest> consume_break_request();
    std::optional<PlaceRequest> consume_place_request();

    void on_action_rejected();
    
private:
    BlockRaycastResult raycast(const World& world, const rf::Vec3& origin, 
                                const rf::Vec3& direction, float max_distance) const;
    float calculate_break_time(BlockType block_type, const ecs::ToolHolder& tool) const;
    
    BlockRaycastResult target_;
    float break_progress_{0.0f};
    bool was_breaking_{false};

    bool was_placing_{false};

    std::optional<BreakRequest> pending_break_;
    std::optional<PlaceRequest> pending_place_;

    std::optional<BreakRequest> outgoing_break_;
    std::optional<PlaceRequest> outgoing_place_;

    std::array<rf::GLTexture, DESTROY_STAGE_COUNT> destroy_textures_;
    bool textures_loaded_{false};

    // Rendering resources (loaded in init())
    rf::GLShader solidShader_;
    rf::GLShader overlayShader_;
    rf::GLMesh   wireframeCube_;
    rf::GLMesh   overlayCube_;    // Solid cube with UVs for destroy overlay
    bool         render_resources_loaded_{false};
};

} // namespace voxel
