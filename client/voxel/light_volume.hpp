#pragma once

#include <cstdint>
#include <vector>

#include "block.hpp"
#include <raylib.h>

namespace voxel {

class World;

// Client-only Minecraft-style lighting volume (skylight + blocklight), bounded around the camera/player.
// Used only for rendering (vertex shade), never for gameplay.
class LightVolume {
public:
    struct Settings {
        int volume_x{64};
        int volume_y{96};
        int volume_z{64};

        int origin_step_voxels{4};
        float max_update_hz{2.0f};
    };

    void set_settings(const Settings& s);
    const Settings& settings() const { return settings_; }

    // Returns true if the volume was rebuilt this call.
    bool update_if_needed(const World& world, const Vector3& center_pos_ws, bool force_rebuild);

    // True if there is any queued relight work pending.
    bool has_pending_relight() const { return relight_active_ || !pending_changes_.empty(); }

    // Incremental relight API (Minecraft-style).
    // When a block changes, call notify_block_changed() to queue relight work.
    // Then call process_pending_relight() periodically with a node budget.
    void notify_block_changed(int wx, int wy, int wz, BlockType old_type, BlockType new_type);
    bool process_pending_relight(const World& world, int budget_nodes = 4096);

    // If process_pending_relight() changed any light values, this returns the
    // world-space bounds that were touched (inclusive) and resets the stored bounds.
    bool consume_dirty_bounds(int& out_min_wx, int& out_min_wy, int& out_min_wz,
                              int& out_max_wx, int& out_max_wy, int& out_max_wz);

    bool ready() const { return have_volume_; }

    // Returns combined light (max of skylight and blocklight) in range [0..15].
    // If outside the current volume, clamps to the nearest edge sample to avoid
    // both dark seams and artificial skylight leaking into interiors.
    std::uint8_t sample_combined(int wx, int wy, int wz) const;

    // Returns combined light in [0..1].
    float sample_combined01(int wx, int wy, int wz) const;

    // Separate channels (Minecraft-style). Range is [0..15] / [0..1].
    // Outside the current volume, clamps to the nearest edge sample.
    std::uint8_t sample_skylight(int wx, int wy, int wz) const;
    std::uint8_t sample_blocklight(int wx, int wy, int wz) const;
    float sample_skylight01(int wx, int wy, int wz) const;
    float sample_blocklight01(int wx, int wy, int wz) const;

    Vector3 volume_origin_ws() const { return volume_origin_ws_; }

private:
    struct QueueNode {
        std::uint16_t x;
        std::uint16_t y;
        std::uint16_t z;
        std::uint8_t level;
    };

    struct PendingChange {
        int wx;
        int wy;
        int wz;
        BlockType old_type;
        BlockType new_type;
    };

    static int floor_div_(int a, int b);

    void rebuild_(const World& world);

    // Incremental rebuild (spread over multiple frames) to avoid long frame stalls.
    void start_rebuild_(int new_origin_x, int new_origin_y, int new_origin_z, bool forced);
    bool step_rebuild_(const World& world, float budget_ms);

    bool in_volume_(int wx, int wy, int wz, int& out_lx, int& out_ly, int& out_lz) const;

    Settings settings_{};

    bool have_volume_{false};

    int origin_x_{0};
    int origin_y_{0};
    int origin_z_{0};
    Vector3 volume_origin_ws_{0.0f, 0.0f, 0.0f};

    double last_update_time_{0.0};

    std::vector<std::uint8_t> skylight_{};
    std::vector<std::uint8_t> blocklight_{};

    // Cached per-voxel opacity for the current volume; avoids calling World::get_block
    // inside BFS propagation.
    std::vector<std::uint8_t> opaque_{};

    // Cached per-voxel attenuation/behavior for BFS propagation.
    // Values are "extra" attenuation (added on top of the base per-step cost of 1).
    std::vector<std::uint8_t> block_atten_{};
    std::vector<std::uint8_t> sky_atten_{};
    std::vector<std::uint8_t> sky_dim_vertical_{};

    // Reused BFS queues to avoid per-rebuild allocations.
    std::vector<QueueNode> q_sky_{};
    std::vector<QueueNode> q_blk_{};

    // Incremental rebuild (back buffers + progress).
    enum class RebuildPhase : std::uint8_t { Scan = 0, BfsSky = 1, BfsBlk = 2 };
    bool rebuild_active_{false};
    bool rebuild_forced_{false};
    float rebuild_work_ms_accum_{0.0f};
    RebuildPhase rebuild_phase_{RebuildPhase::Scan};
    std::size_t rebuild_scan_i_{0};
    std::size_t rebuild_head_sky_{0};
    std::size_t rebuild_head_blk_{0};
    int rebuild_origin_x_{0};
    int rebuild_origin_y_{0};
    int rebuild_origin_z_{0};
    Vector3 rebuild_origin_ws_{0.0f, 0.0f, 0.0f};

    std::vector<std::uint8_t> skylight_back_{};
    std::vector<std::uint8_t> blocklight_back_{};
    std::vector<std::uint8_t> opaque_back_{};
    std::vector<std::uint8_t> block_atten_back_{};
    std::vector<std::uint8_t> sky_atten_back_{};
    std::vector<std::uint8_t> sky_dim_vertical_back_{};
    std::vector<QueueNode> q_sky_back_{};
    std::vector<QueueNode> q_blk_back_{};

    // Block changes observed while a rebuild is in-flight; applied after swap.
    std::vector<PendingChange> pending_changes_during_rebuild_{};

    // Incremental relight state.
    std::vector<PendingChange> pending_changes_{};
    PendingChange active_change_{};
    bool relight_active_{false};

    std::vector<QueueNode> q_dec_sky_{};
    std::vector<QueueNode> q_inc_sky_{};
    std::vector<QueueNode> q_dec_blk_{};
    std::vector<QueueNode> q_inc_blk_{};

    std::size_t head_dec_sky_{0};
    std::size_t head_inc_sky_{0};
    std::size_t head_dec_blk_{0};
    std::size_t head_inc_blk_{0};

    bool dirty_bounds_valid_{false};
    int dirty_min_wx_{0}, dirty_min_wy_{0}, dirty_min_wz_{0};
    int dirty_max_wx_{0}, dirty_max_wy_{0}, dirty_max_wz_{0};
};

} // namespace voxel
