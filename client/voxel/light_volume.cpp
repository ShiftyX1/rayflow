#include "light_volume.hpp"

#include "world.hpp"
#include "block.hpp"

#include "../core/config.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <utility>

namespace voxel {

void LightVolume::set_settings(const Settings& s) {
    settings_ = s;
    have_volume_ = false;
    last_update_time_ = 0.0;
    skylight_.clear();
    blocklight_.clear();
    opaque_.clear();
    block_atten_.clear();
    sky_atten_.clear();
    sky_dim_vertical_.clear();
    q_sky_.clear();
    q_blk_.clear();

    pending_changes_.clear();
    relight_active_ = false;
    q_dec_sky_.clear();
    q_inc_sky_.clear();
    q_dec_blk_.clear();
    q_inc_blk_.clear();
    head_dec_sky_ = 0;
    head_inc_sky_ = 0;
    head_dec_blk_ = 0;
    head_inc_blk_ = 0;
    dirty_bounds_valid_ = false;

    rebuild_active_ = false;
    rebuild_phase_ = RebuildPhase::Scan;
    rebuild_scan_i_ = 0;
    rebuild_head_sky_ = 0;
    rebuild_head_blk_ = 0;
    pending_changes_during_rebuild_.clear();
}

int LightVolume::floor_div_(int a, int b) {
    if (b <= 0) return 0;
    if (a >= 0) return a / b;
    return -static_cast<int>((static_cast<unsigned int>(-a) + static_cast<unsigned int>(b) - 1u) / static_cast<unsigned int>(b));
}

bool LightVolume::update_if_needed(const World& world, const Vector3& center_pos_ws, bool force_rebuild) {
    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int step = std::max(1, settings_.origin_step_voxels);

    const int cx = static_cast<int>(std::floor(center_pos_ws.x));
    const int cy = static_cast<int>(std::floor(center_pos_ws.y));
    const int cz = static_cast<int>(std::floor(center_pos_ws.z));

    int ox = cx - dim_x / 2;
    int oy = cy - dim_y / 2;
    int oz = cz - dim_z / 2;

    ox = floor_div_(ox, step) * step;
    oy = floor_div_(oy, step) * step;
    oz = floor_div_(oz, step) * step;

    const bool origin_changed = (!have_volume_) || ox != origin_x_ || oy != origin_y_ || oz != origin_z_;

    const double now = GetTime();
    const double min_dt = (settings_.max_update_hz <= 0.0f) ? 0.0 : (1.0 / static_cast<double>(settings_.max_update_hz));
    const bool rate_ok = (now - last_update_time_) >= min_dt;

    // When the volume is missing (startup) or explicitly forced, spend more time per frame
    // so we converge faster and avoid long periods of incorrect/black lighting.
    const float rebuild_budget_ms = (!have_volume_ || force_rebuild) ? 6.0f : 2.0f;

    if (rebuild_active_) {
        if (ox != rebuild_origin_x_ || oy != rebuild_origin_y_ || oz != rebuild_origin_z_) {
            start_rebuild_(ox, oy, oz, rebuild_forced_ || force_rebuild);
        }

        if (step_rebuild_(world, rebuild_budget_ms)) {
            last_update_time_ = now;
            return true;
        }
        return false;
    }

    if ((!origin_changed && !force_rebuild) || !rate_ok) {
        return false;
    }

    start_rebuild_(ox, oy, oz, force_rebuild);
    if (step_rebuild_(world, rebuild_budget_ms)) {
        last_update_time_ = now;
        return true;
    }
    return false;
}

bool LightVolume::in_volume_(int wx, int wy, int wz, int& out_lx, int& out_ly, int& out_lz) const {
    if (!have_volume_) return false;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int lx = wx - origin_x_;
    const int ly = wy - origin_y_;
    const int lz = wz - origin_z_;

    if (lx < 0 || ly < 0 || lz < 0) return false;
    if (lx >= dim_x || ly >= dim_y || lz >= dim_z) return false;

    out_lx = lx;
    out_ly = ly;
    out_lz = lz;
    return true;
}

void LightVolume::notify_block_changed(int wx, int wy, int wz, BlockType old_type, BlockType new_type) {
    if (!have_volume_) return;

    int lx = 0, ly = 0, lz = 0;
    if (!in_volume_(wx, wy, wz, lx, ly, lz)) return;

    pending_changes_.push_back(PendingChange{ wx, wy, wz, old_type, new_type });
    if (rebuild_active_) {
        pending_changes_during_rebuild_.push_back(PendingChange{ wx, wy, wz, old_type, new_type });
    }
}

void LightVolume::start_rebuild_(int new_origin_x, int new_origin_y, int new_origin_z, bool forced) {
    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    rebuild_active_ = true;
    rebuild_forced_ = forced;
    rebuild_work_ms_accum_ = 0.0f;
    rebuild_phase_ = RebuildPhase::Scan;
    rebuild_scan_i_ = 0;
    rebuild_head_sky_ = 0;
    rebuild_head_blk_ = 0;

    rebuild_origin_x_ = new_origin_x;
    rebuild_origin_y_ = new_origin_y;
    rebuild_origin_z_ = new_origin_z;
    rebuild_origin_ws_ = { static_cast<float>(new_origin_x), static_cast<float>(new_origin_y), static_cast<float>(new_origin_z) };

    const std::size_t volume_size = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_y) * static_cast<std::size_t>(dim_z);
    if (skylight_back_.size() != volume_size) skylight_back_.resize(volume_size);
    if (blocklight_back_.size() != volume_size) blocklight_back_.resize(volume_size);
    if (opaque_back_.size() != volume_size) opaque_back_.resize(volume_size);
    if (block_atten_back_.size() != volume_size) block_atten_back_.resize(volume_size);
    if (sky_atten_back_.size() != volume_size) sky_atten_back_.resize(volume_size);
    if (sky_dim_vertical_back_.size() != volume_size) sky_dim_vertical_back_.resize(volume_size);

    if (q_sky_back_.capacity() < volume_size) q_sky_back_.reserve(volume_size);
    if (q_blk_back_.capacity() < volume_size) q_blk_back_.reserve(volume_size);
    q_sky_back_.clear();
    q_blk_back_.clear();

    pending_changes_during_rebuild_.clear();
}

bool LightVolume::step_rebuild_(const World& world, float budget_ms) {
    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const std::size_t stride_z = static_cast<std::size_t>(dim_x);
    const std::size_t stride_y = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);
    const auto idx = [stride_z, stride_y](int x, int y, int z) -> std::size_t {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(z) * stride_z + static_cast<std::size_t>(y) * stride_y;
    };

    const std::size_t volume_size = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_y) * static_cast<std::size_t>(dim_z);
    const int top_y = dim_y - 1;

    const auto t0 = std::chrono::steady_clock::now();
    auto budget_ok = [t0, budget_ms]() {
        const auto t1 = std::chrono::steady_clock::now();
        const float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
        return ms < budget_ms;
    };

    while (rebuild_phase_ == RebuildPhase::Scan && rebuild_scan_i_ < volume_size && budget_ok()) {
        const std::size_t i = rebuild_scan_i_++;

        const int x = static_cast<int>(i % static_cast<std::size_t>(dim_x));
        const int z = static_cast<int>((i / static_cast<std::size_t>(dim_x)) % static_cast<std::size_t>(dim_z));
        const int y = static_cast<int>(i / (static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z)));

        const int wx = rebuild_origin_x_ + x;
        const int wy = rebuild_origin_y_ + y;
        const int wz = rebuild_origin_z_ + z;

        const BlockType bt = static_cast<BlockType>(world.get_block(wx, wy, wz));
        const auto& props = shared::voxel::get_light_props(static_cast<shared::voxel::BlockType>(bt));
        const bool opaque = props.opaqueForLight;

        skylight_back_[i] = 0u;
        blocklight_back_[i] = 0u;
        opaque_back_[i] = opaque ? 1u : 0u;
        block_atten_back_[i] = props.blockAttenuation;
        sky_atten_back_[i] = props.skyAttenuation;
        sky_dim_vertical_back_[i] = props.skyDimVertical ? 1u : 0u;

        if (!opaque && y == top_y) {
            skylight_back_[i] = 15u;
            q_sky_back_.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), 15u });
        }

        if (props.emission > 0u && !opaque) {
            blocklight_back_[i] = props.emission;
            q_blk_back_.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), props.emission });
        }
    }

    if (rebuild_phase_ == RebuildPhase::Scan && rebuild_scan_i_ >= volume_size) {
        rebuild_phase_ = RebuildPhase::BfsSky;
        rebuild_head_sky_ = 0;
    }

    auto try_push_sky = [idx, dim_x, dim_y, dim_z, this](
                            std::vector<QueueNode>& q,
                            int x, int y, int z,
                            std::uint8_t from_level,
                            bool is_down) {
        if (from_level == 0) return;
        if (x < 0 || y < 0 || z < 0) return;
        if (x >= dim_x || y >= dim_y || z >= dim_z) return;

        const std::size_t i = idx(x, y, z);
        if (opaque_back_[i] != 0u) return;

        const std::uint8_t extra = sky_atten_back_[i];
        const std::uint8_t base = is_down ? (sky_dim_vertical_back_[i] != 0u ? 1u : 0u) : 1u;
        const std::uint8_t cost = static_cast<std::uint8_t>(base + extra);
        const std::uint8_t new_level = (from_level > cost) ? static_cast<std::uint8_t>(from_level - cost) : 0u;
        if (new_level == 0u) return;
        if (new_level <= skylight_back_[i]) return;

        skylight_back_[i] = new_level;
        q.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), new_level });
    };

    auto try_push_blk = [idx, dim_x, dim_y, dim_z, this](
                            std::vector<QueueNode>& q,
                            int x, int y, int z,
                            std::uint8_t from_level) {
        if (from_level <= 1u) return;
        if (x < 0 || y < 0 || z < 0) return;
        if (x >= dim_x || y >= dim_y || z >= dim_z) return;

        const std::size_t i = idx(x, y, z);
        if (opaque_back_[i] != 0u) return;

        const std::uint8_t cost = static_cast<std::uint8_t>(1u + block_atten_back_[i]);
        const std::uint8_t new_level = (from_level > cost) ? static_cast<std::uint8_t>(from_level - cost) : 0u;
        if (new_level == 0u) return;
        if (new_level <= blocklight_back_[i]) return;

        blocklight_back_[i] = new_level;
        q.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), new_level });
    };

    while (rebuild_phase_ == RebuildPhase::BfsSky && rebuild_head_sky_ < q_sky_back_.size() && budget_ok()) {
        const QueueNode n = q_sky_back_[rebuild_head_sky_++];
        if (n.level == 0) continue;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        try_push_sky(q_sky_back_, x, y - 1, z, n.level, true);
        try_push_sky(q_sky_back_, x + 1, y, z, n.level, false);
        try_push_sky(q_sky_back_, x - 1, y, z, n.level, false);
        try_push_sky(q_sky_back_, x, y, z + 1, n.level, false);
        try_push_sky(q_sky_back_, x, y, z - 1, n.level, false);
        try_push_sky(q_sky_back_, x, y + 1, z, n.level, false);
    }

    if (rebuild_phase_ == RebuildPhase::BfsSky && rebuild_head_sky_ >= q_sky_back_.size()) {
        rebuild_phase_ = RebuildPhase::BfsBlk;
        rebuild_head_blk_ = 0;
    }

    while (rebuild_phase_ == RebuildPhase::BfsBlk && rebuild_head_blk_ < q_blk_back_.size() && budget_ok()) {
        const QueueNode n = q_blk_back_[rebuild_head_blk_++];
        if (n.level <= 1u) continue;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        try_push_blk(q_blk_back_, x + 1, y, z, n.level);
        try_push_blk(q_blk_back_, x - 1, y, z, n.level);
        try_push_blk(q_blk_back_, x, y + 1, z, n.level);
        try_push_blk(q_blk_back_, x, y - 1, z, n.level);
        try_push_blk(q_blk_back_, x, y, z + 1, n.level);
        try_push_blk(q_blk_back_, x, y, z - 1, n.level);
    }

    if (rebuild_phase_ != RebuildPhase::BfsBlk || rebuild_head_blk_ < q_blk_back_.size()) {
        const auto t1 = std::chrono::steady_clock::now();
        rebuild_work_ms_accum_ += std::chrono::duration<float, std::milli>(t1 - t0).count();
        return false;
    }

    const auto t1 = std::chrono::steady_clock::now();
    rebuild_work_ms_accum_ += std::chrono::duration<float, std::milli>(t1 - t0).count();
    const float ms = rebuild_work_ms_accum_;
    const auto& prof = core::Config::instance().profiling();
    if (prof.enabled && prof.light_volume) {
        static double last_log_s = 0.0;
        const double now_s = GetTime();
        const bool interval_ok = prof.log_every_event || ((now_s - last_log_s) * 1000.0 >= static_cast<double>(std::max(0, prof.log_interval_ms)));
        if (ms >= prof.warn_light_volume_ms && interval_ok) {
            TraceLog(LOG_INFO, "[prof] light_volume rebuild: %.2f ms (dim=%dx%dx%d, force=%s)",
                     ms, dim_x, dim_y, dim_z, rebuild_forced_ ? "true" : "false");
            last_log_s = now_s;
        }
    }

    origin_x_ = rebuild_origin_x_;
    origin_y_ = rebuild_origin_y_;
    origin_z_ = rebuild_origin_z_;
    volume_origin_ws_ = rebuild_origin_ws_;

    std::swap(skylight_, skylight_back_);
    std::swap(blocklight_, blocklight_back_);
    std::swap(opaque_, opaque_back_);
    std::swap(block_atten_, block_atten_back_);
    std::swap(sky_atten_, sky_atten_back_);
    std::swap(sky_dim_vertical_, sky_dim_vertical_back_);
    std::swap(q_sky_, q_sky_back_);
    std::swap(q_blk_, q_blk_back_);

    have_volume_ = true;
    rebuild_active_ = false;

    if (!pending_changes_during_rebuild_.empty()) {
        pending_changes_.insert(pending_changes_.end(), pending_changes_during_rebuild_.begin(), pending_changes_during_rebuild_.end());
        pending_changes_during_rebuild_.clear();
    }

    relight_active_ = false;
    q_dec_sky_.clear();
    q_inc_sky_.clear();
    q_dec_blk_.clear();
    q_inc_blk_.clear();
    head_dec_sky_ = 0;
    head_inc_sky_ = 0;
    head_dec_blk_ = 0;
    head_inc_blk_ = 0;
    dirty_bounds_valid_ = false;

    return true;
}

bool LightVolume::consume_dirty_bounds(int& out_min_wx, int& out_min_wy, int& out_min_wz,
                                      int& out_max_wx, int& out_max_wy, int& out_max_wz) {
    if (!dirty_bounds_valid_) return false;
    out_min_wx = dirty_min_wx_;
    out_min_wy = dirty_min_wy_;
    out_min_wz = dirty_min_wz_;
    out_max_wx = dirty_max_wx_;
    out_max_wy = dirty_max_wy_;
    out_max_wz = dirty_max_wz_;
    dirty_bounds_valid_ = false;
    return true;
}

bool LightVolume::process_pending_relight(const World& world, int budget_nodes) {
    if (!have_volume_) return false;
    if (budget_nodes <= 0) return false;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const std::size_t stride_z = static_cast<std::size_t>(dim_x);
    const std::size_t stride_y = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);
    const auto idx = [stride_z, stride_y](int x, int y, int z) -> std::size_t {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(z) * stride_z + static_cast<std::size_t>(y) * stride_y;
    };

    const std::size_t volume_size = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_y) * static_cast<std::size_t>(dim_z);
    if (q_dec_sky_.capacity() < volume_size) q_dec_sky_.reserve(volume_size);
    if (q_inc_sky_.capacity() < volume_size) q_inc_sky_.reserve(volume_size);
    if (q_dec_blk_.capacity() < volume_size) q_dec_blk_.reserve(volume_size);
    if (q_inc_blk_.capacity() < volume_size) q_inc_blk_.reserve(volume_size);

    bool any_changed = false;

    auto note_dirty = [this](int wx, int wy, int wz) {
        if (!dirty_bounds_valid_) {
            dirty_bounds_valid_ = true;
            dirty_min_wx_ = dirty_max_wx_ = wx;
            dirty_min_wy_ = dirty_max_wy_ = wy;
            dirty_min_wz_ = dirty_max_wz_ = wz;
            return;
        }
        dirty_min_wx_ = std::min(dirty_min_wx_, wx);
        dirty_min_wy_ = std::min(dirty_min_wy_, wy);
        dirty_min_wz_ = std::min(dirty_min_wz_, wz);
        dirty_max_wx_ = std::max(dirty_max_wx_, wx);
        dirty_max_wy_ = std::max(dirty_max_wy_, wy);
        dirty_max_wz_ = std::max(dirty_max_wz_, wz);
    };

    auto in_bounds = [dim_x, dim_y, dim_z](int x, int y, int z) {
        return x >= 0 && y >= 0 && z >= 0 && x < dim_x && y < dim_y && z < dim_z;
    };

    auto seed_from_cell = [idx, this, &in_bounds](std::vector<QueueNode>& q, const std::vector<std::uint8_t>& channel, int x, int y, int z) {
        if (!in_bounds(x, y, z)) return;
        const std::size_t i = idx(x, y, z);
        const std::uint8_t level = channel[i];
        if (level == 0u) return;
        q.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), level });
    };

    const int top_y = dim_y - 1;

    if (!relight_active_) {
        if (pending_changes_.empty()) return false;

        q_dec_sky_.clear();
        q_inc_sky_.clear();
        q_dec_blk_.clear();
        q_inc_blk_.clear();
        head_dec_sky_ = 0;
        head_inc_sky_ = 0;
        head_dec_blk_ = 0;
        head_inc_blk_ = 0;

        // Sort pending changes by distance from volume center (camera-prioritized relight).
        // Closer changes are processed first for instant visual feedback.
        const int center_wx = origin_x_ + dim_x / 2;
        const int center_wy = origin_y_ + dim_y / 2;
        const int center_wz = origin_z_ + dim_z / 2;
        std::sort(pending_changes_.begin(), pending_changes_.end(),
            [center_wx, center_wy, center_wz](const PendingChange& a, const PendingChange& b) {
                const int da = (a.wx - center_wx) * (a.wx - center_wx) +
                               (a.wy - center_wy) * (a.wy - center_wy) +
                               (a.wz - center_wz) * (a.wz - center_wz);
                const int db = (b.wx - center_wx) * (b.wx - center_wx) +
                               (b.wy - center_wy) * (b.wy - center_wy) +
                               (b.wz - center_wz) * (b.wz - center_wz);
                return da < db;
            });

        struct ChangeL {
            int wx, wy, wz;
            int lx, ly, lz;
            shared::voxel::BlockLightProps props;
        };

        struct Col {
            std::uint16_t lx;
            std::uint16_t lz;
        };

        constexpr std::size_t kBatchMax = 256;
        const std::size_t take = std::min(kBatchMax, pending_changes_.size());

        std::vector<ChangeL> changes;
        changes.reserve(take);

        std::vector<Col> columns;
        columns.reserve(take);

        for (std::size_t n = 0; n < take; ++n) {
            const PendingChange& ch = pending_changes_[n];

            int lx = 0, ly = 0, lz = 0;
            if (!in_volume_(ch.wx, ch.wy, ch.wz, lx, ly, lz)) {
                continue;
            }

            const BlockType bt = static_cast<BlockType>(world.get_block(ch.wx, ch.wy, ch.wz));
            const auto& props = shared::voxel::get_light_props(static_cast<shared::voxel::BlockType>(bt));

            changes.push_back(ChangeL{ ch.wx, ch.wy, ch.wz, lx, ly, lz, props });
            columns.push_back(Col{ static_cast<std::uint16_t>(lx), static_cast<std::uint16_t>(lz) });
        }

        pending_changes_.erase(pending_changes_.begin(), pending_changes_.begin() + static_cast<std::ptrdiff_t>(take));
        if (changes.empty()) return false;

        for (const auto& ch : changes) {
            const std::size_t ci = idx(ch.lx, ch.ly, ch.lz);
            opaque_[ci] = ch.props.opaqueForLight ? 1u : 0u;
            block_atten_[ci] = ch.props.blockAttenuation;
            sky_atten_[ci] = ch.props.skyAttenuation;
            sky_dim_vertical_[ci] = ch.props.skyDimVertical ? 1u : 0u;

            const std::uint8_t old_blk = blocklight_[ci];
            if (old_blk != 0u) {
                blocklight_[ci] = 0u;
                q_dec_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(ch.lx), static_cast<std::uint16_t>(ch.ly), static_cast<std::uint16_t>(ch.lz), old_blk });
                note_dirty(ch.wx, ch.wy, ch.wz);
                any_changed = true;
            }
        }

        std::sort(columns.begin(), columns.end(), [](const Col& a, const Col& b) {
            if (a.lx != b.lx) return a.lx < b.lx;
            return a.lz < b.lz;
        });
        columns.erase(std::unique(columns.begin(), columns.end(), [](const Col& a, const Col& b) {
            return a.lx == b.lx && a.lz == b.lz;
        }), columns.end());

        for (const Col& c : columns) {
            const int lx = static_cast<int>(c.lx);
            const int lz = static_cast<int>(c.lz);

            std::uint8_t column_level = 0u;
            for (int cy = top_y; cy >= 0; --cy) {
                const std::size_t i = idx(lx, cy, lz);
                const std::uint8_t old_level = skylight_[i];

                if (opaque_[i] != 0u) {
                    column_level = 0u;
                } else {
                    if (cy == top_y) {
                        column_level = 15u;
                    } else {
                        const std::uint8_t extra = sky_atten_[i];
                        const std::uint8_t base = (sky_dim_vertical_[i] != 0u) ? 1u : 0u;
                        const std::uint8_t cost = static_cast<std::uint8_t>(base + extra);
                        column_level = (column_level > cost) ? static_cast<std::uint8_t>(column_level - cost) : 0u;
                    }
                }

                if (old_level == column_level) continue;

                skylight_[i] = column_level;
                note_dirty(origin_x_ + lx, origin_y_ + cy, origin_z_ + lz);
                any_changed = true;

                if (old_level > column_level) {
                    q_dec_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(lx), static_cast<std::uint16_t>(cy), static_cast<std::uint16_t>(lz), old_level });
                } else {
                    q_inc_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(lx), static_cast<std::uint16_t>(cy), static_cast<std::uint16_t>(lz), column_level });
                }
            }
        }

        for (const auto& ch : changes) {
            const int lx = ch.lx;
            const int ly = ch.ly;
            const int lz = ch.lz;

            seed_from_cell(q_inc_blk_, blocklight_, lx, ly, lz);
            seed_from_cell(q_inc_blk_, blocklight_, lx + 1, ly, lz);
            seed_from_cell(q_inc_blk_, blocklight_, lx - 1, ly, lz);
            seed_from_cell(q_inc_blk_, blocklight_, lx, ly + 1, lz);
            seed_from_cell(q_inc_blk_, blocklight_, lx, ly - 1, lz);
            seed_from_cell(q_inc_blk_, blocklight_, lx, ly, lz + 1);
            seed_from_cell(q_inc_blk_, blocklight_, lx, ly, lz - 1);

            seed_from_cell(q_inc_sky_, skylight_, lx, ly, lz);
            seed_from_cell(q_inc_sky_, skylight_, lx + 1, ly, lz);
            seed_from_cell(q_inc_sky_, skylight_, lx - 1, ly, lz);
            seed_from_cell(q_inc_sky_, skylight_, lx, ly + 1, lz);
            seed_from_cell(q_inc_sky_, skylight_, lx, ly - 1, lz);
            seed_from_cell(q_inc_sky_, skylight_, lx, ly, lz + 1);
            seed_from_cell(q_inc_sky_, skylight_, lx, ly, lz - 1);

            const std::size_t ci = idx(lx, ly, lz);
            if (opaque_[ci] == 0u && ch.props.emission > 0u) {
                if (ch.props.emission > blocklight_[ci]) {
                    blocklight_[ci] = ch.props.emission;
                    q_inc_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(lx), static_cast<std::uint16_t>(ly), static_cast<std::uint16_t>(lz), ch.props.emission });
                    note_dirty(ch.wx, ch.wy, ch.wz);
                    any_changed = true;
                }
            }
        }

        relight_active_ = true;
    }

    auto sky_cost = [idx, this](int nx, int ny, int nz, bool is_down) -> std::uint8_t {
        const std::size_t i = idx(nx, ny, nz);
        const std::uint8_t extra = sky_atten_[i];
        const std::uint8_t base = is_down ? (sky_dim_vertical_[i] != 0u ? 1u : 0u) : 1u;
        return static_cast<std::uint8_t>(base + extra);
    };

    auto blk_cost = [idx, this](int nx, int ny, int nz) -> std::uint8_t {
        const std::size_t i = idx(nx, ny, nz);
        return static_cast<std::uint8_t>(1u + block_atten_[i]);
    };

    auto neighbors6 = [](int x, int y, int z, int out_xyz[6][3]) {
        out_xyz[0][0] = x + 1; out_xyz[0][1] = y;     out_xyz[0][2] = z;
        out_xyz[1][0] = x - 1; out_xyz[1][1] = y;     out_xyz[1][2] = z;
        out_xyz[2][0] = x;     out_xyz[2][1] = y + 1; out_xyz[2][2] = z;
        out_xyz[3][0] = x;     out_xyz[3][1] = y - 1; out_xyz[3][2] = z;
        out_xyz[4][0] = x;     out_xyz[4][1] = y;     out_xyz[4][2] = z + 1;
        out_xyz[5][0] = x;     out_xyz[5][1] = y;     out_xyz[5][2] = z - 1;
    };

    while (budget_nodes > 0 && head_dec_blk_ < q_dec_blk_.size()) {
        const QueueNode n = q_dec_blk_[head_dec_blk_++];
        --budget_nodes;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        int neigh[6][3];
        neighbors6(x, y, z, neigh);

        for (int k = 0; k < 6; ++k) {
            const int nx = neigh[k][0];
            const int ny = neigh[k][1];
            const int nz = neigh[k][2];
            if (!in_bounds(nx, ny, nz)) continue;

            const std::size_t ni = idx(nx, ny, nz);
            const std::uint8_t nl = blocklight_[ni];
            if (nl == 0u) continue;

            const std::uint8_t cost = blk_cost(nx, ny, nz);
            const std::uint8_t expected = (n.level > cost) ? static_cast<std::uint8_t>(n.level - cost) : 0u;
            if (expected == 0u) continue;

            if (nl <= expected) {
                blocklight_[ni] = 0u;
                q_dec_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny), static_cast<std::uint16_t>(nz), nl });
                note_dirty(origin_x_ + nx, origin_y_ + ny, origin_z_ + nz);
                any_changed = true;
            } else {
                q_inc_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny), static_cast<std::uint16_t>(nz), nl });
            }
        }
    }

    while (budget_nodes > 0 && head_dec_sky_ < q_dec_sky_.size()) {
        const QueueNode n = q_dec_sky_[head_dec_sky_++];
        --budget_nodes;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        int neigh[6][3];
        neighbors6(x, y, z, neigh);

        for (int k = 0; k < 6; ++k) {
            const int nx = neigh[k][0];
            const int ny = neigh[k][1];
            const int nz = neigh[k][2];
            if (!in_bounds(nx, ny, nz)) continue;

            const std::size_t ni = idx(nx, ny, nz);
            const std::uint8_t nl = skylight_[ni];
            if (nl == 0u) continue;

            const bool is_down = (ny == y - 1);
            const std::uint8_t cost = sky_cost(nx, ny, nz, is_down);
            const std::uint8_t expected = (n.level > cost) ? static_cast<std::uint8_t>(n.level - cost) : 0u;
            if (expected == 0u) continue;

            if (nl <= expected) {
                skylight_[ni] = 0u;
                q_dec_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny), static_cast<std::uint16_t>(nz), nl });
                note_dirty(origin_x_ + nx, origin_y_ + ny, origin_z_ + nz);
                any_changed = true;
            } else {
                q_inc_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny), static_cast<std::uint16_t>(nz), nl });
            }
        }
    }

    while (budget_nodes > 0 && head_inc_sky_ < q_inc_sky_.size()) {
        const QueueNode n = q_inc_sky_[head_inc_sky_++];
        --budget_nodes;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        int neigh[6][3];
        neighbors6(x, y, z, neigh);

        for (int k = 0; k < 6; ++k) {
            const int nx = neigh[k][0];
            const int ny = neigh[k][1];
            const int nz = neigh[k][2];
            if (!in_bounds(nx, ny, nz)) continue;

            const std::size_t ni = idx(nx, ny, nz);
            if (opaque_[ni] != 0u) continue;

            const bool is_down = (ny == y - 1);
            const std::uint8_t cost = sky_cost(nx, ny, nz, is_down);
            const std::uint8_t candidate = (n.level > cost) ? static_cast<std::uint8_t>(n.level - cost) : 0u;
            if (candidate == 0u) continue;

            if (candidate > skylight_[ni]) {
                skylight_[ni] = candidate;
                q_inc_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny), static_cast<std::uint16_t>(nz), candidate });
                note_dirty(origin_x_ + nx, origin_y_ + ny, origin_z_ + nz);
                any_changed = true;
            }
        }
    }

    while (budget_nodes > 0 && head_inc_blk_ < q_inc_blk_.size()) {
        const QueueNode n = q_inc_blk_[head_inc_blk_++];
        --budget_nodes;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        int neigh[6][3];
        neighbors6(x, y, z, neigh);

        for (int k = 0; k < 6; ++k) {
            const int nx = neigh[k][0];
            const int ny = neigh[k][1];
            const int nz = neigh[k][2];
            if (!in_bounds(nx, ny, nz)) continue;

            const std::size_t ni = idx(nx, ny, nz);
            if (opaque_[ni] != 0u) continue;

            const std::uint8_t cost = blk_cost(nx, ny, nz);
            const std::uint8_t candidate = (n.level > cost) ? static_cast<std::uint8_t>(n.level - cost) : 0u;
            if (candidate == 0u) continue;

            if (candidate > blocklight_[ni]) {
                blocklight_[ni] = candidate;
                q_inc_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(nx), static_cast<std::uint16_t>(ny), static_cast<std::uint16_t>(nz), candidate });
                note_dirty(origin_x_ + nx, origin_y_ + ny, origin_z_ + nz);
                any_changed = true;
            }
        }
    }

    // If all queues are drained, finish this change.
    if (head_dec_blk_ >= q_dec_blk_.size() && head_dec_sky_ >= q_dec_sky_.size() &&
        head_inc_blk_ >= q_inc_blk_.size() && head_inc_sky_ >= q_inc_sky_.size()) {
        relight_active_ = false;
        q_dec_blk_.clear();
        q_dec_sky_.clear();
        q_inc_blk_.clear();
        q_inc_sky_.clear();
        head_dec_blk_ = head_dec_sky_ = head_inc_blk_ = head_inc_sky_ = 0;
    }

    return any_changed;
}

void LightVolume::rebuild_(const World& world) {
    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const std::size_t stride_z = static_cast<std::size_t>(dim_x);
    const std::size_t stride_y = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);
    const auto idx = [stride_z, stride_y](int x, int y, int z) -> std::size_t {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(z) * stride_z + static_cast<std::size_t>(y) * stride_y;
    };

    const std::size_t volume_size = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_y) * static_cast<std::size_t>(dim_z);
    if (q_sky_.capacity() < volume_size) q_sky_.reserve(volume_size);
    if (q_blk_.capacity() < volume_size) q_blk_.reserve(volume_size);
    q_sky_.clear();
    q_blk_.clear();

    // Precompute opacity and seed sources in a single scan.
    // This avoids calling World::get_block in the BFS inner loop.
    const int top_y = dim_y - 1;
    for (int y = 0; y < dim_y; ++y) {
        for (int z = 0; z < dim_z; ++z) {
            for (int x = 0; x < dim_x; ++x) {
                const int wx = origin_x_ + x;
                const int wy = origin_y_ + y;
                const int wz = origin_z_ + z;

                const BlockType bt = static_cast<BlockType>(world.get_block(wx, wy, wz));
                const auto& props = shared::voxel::get_light_props(static_cast<shared::voxel::BlockType>(bt));
                const bool opaque = props.opaqueForLight;
                const std::size_t i = idx(x, y, z);
                opaque_[i] = opaque ? 1u : 0u;
                block_atten_[i] = props.blockAttenuation;
                sky_atten_[i] = props.skyAttenuation;
                sky_dim_vertical_[i] = props.skyDimVertical ? 1u : 0u;

                if (!opaque && y == top_y) {
                    skylight_[i] = 15u;
                    q_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), 15u });
                }

                if (props.emission > 0u && !opaque) {
                    blocklight_[i] = props.emission;
                    q_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), props.emission });
                }
            }
        }
    }

    auto try_push_sky = [idx, dim_x, dim_y, dim_z, this](
                            std::vector<QueueNode>& q,
                            int x, int y, int z,
                            std::uint8_t from_level,
                            bool is_down) {
        if (from_level == 0) return;
        if (x < 0 || y < 0 || z < 0) return;
        if (x >= dim_x || y >= dim_y || z >= dim_z) return;

        const std::size_t i = idx(x, y, z);
        if (opaque_[i] != 0u) return;

        const std::uint8_t extra = sky_atten_[i];
        const std::uint8_t base = is_down ? (sky_dim_vertical_[i] != 0u ? 1u : 0u) : 1u;
        const std::uint8_t cost = static_cast<std::uint8_t>(base + extra);
        const std::uint8_t new_level = (from_level > cost) ? static_cast<std::uint8_t>(from_level - cost) : 0u;
        if (new_level == 0u) return;
        if (new_level <= skylight_[i]) return;

        skylight_[i] = new_level;
        q.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), new_level });
    };

    auto try_push_blk = [idx, dim_x, dim_y, dim_z, this](
                            std::vector<QueueNode>& q,
                            int x, int y, int z,
                            std::uint8_t from_level) {
        if (from_level <= 1u) return;
        if (x < 0 || y < 0 || z < 0) return;
        if (x >= dim_x || y >= dim_y || z >= dim_z) return;

        const std::size_t i = idx(x, y, z);
        if (opaque_[i] != 0u) return;

        const std::uint8_t cost = static_cast<std::uint8_t>(1u + block_atten_[i]);
        const std::uint8_t new_level = (from_level > cost) ? static_cast<std::uint8_t>(from_level - cost) : 0u;
        if (new_level == 0u) return;
        if (new_level <= blocklight_[i]) return;

        blocklight_[i] = new_level;
        q.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), new_level });
    };

    // BFS skylight
    for (std::size_t head = 0; head < q_sky_.size(); ++head) {
        const QueueNode n = q_sky_[head];

        if (n.level == 0) continue;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        // 6-neighborhood
        // Minecraft-style skylight:
        // - downward keeps same level through normal transparent blocks
        // - "vertical dim" blocks (leaves/water) reduce by 1 when going downward into them
        // - all non-down directions reduce by 1 (plus any extra sky attenuation)
        try_push_sky(q_sky_, x, y - 1, z, n.level, true);
        try_push_sky(q_sky_, x + 1, y, z, n.level, false);
        try_push_sky(q_sky_, x - 1, y, z, n.level, false);
        try_push_sky(q_sky_, x, y, z + 1, n.level, false);
        try_push_sky(q_sky_, x, y, z - 1, n.level, false);
        try_push_sky(q_sky_, x, y + 1, z, n.level, false);
    }

    // BFS blocklight
    for (std::size_t head = 0; head < q_blk_.size(); ++head) {
        const QueueNode n = q_blk_[head];

        if (n.level <= 1u) continue;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        try_push_blk(q_blk_, x + 1, y, z, n.level);
        try_push_blk(q_blk_, x - 1, y, z, n.level);
        try_push_blk(q_blk_, x, y + 1, z, n.level);
        try_push_blk(q_blk_, x, y - 1, z, n.level);
        try_push_blk(q_blk_, x, y, z + 1, n.level);
        try_push_blk(q_blk_, x, y, z - 1, n.level);
    }
}

std::uint8_t LightVolume::sample_combined(int wx, int wy, int wz) const {
    if (!have_volume_) return 15u;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int lx = wx - origin_x_;
    const int ly = wy - origin_y_;
    const int lz = wz - origin_z_;

    // Check if completely outside volume.
    const bool outside_x = lx < 0 || lx >= dim_x;
    const bool outside_y = ly < 0 || ly >= dim_y;
    const bool outside_z = lz < 0 || lz >= dim_z;

    if (outside_x || outside_y || outside_z) {
        // Fallback for blocks outside volume:
        // - Blocks above volume top: assume open sky (skylight=15)
        // - Blocks below/beside: use edge sample to avoid dark seams
        if (ly >= dim_y) {
            // Above volume top - likely open sky, return full skylight.
            return 15u;
        }
        // For other out-of-bounds, clamp to edge sample.
    }

    const int cx = std::clamp(lx, 0, dim_x - 1);
    const int cy = std::clamp(ly, 0, dim_y - 1);
    const int cz = std::clamp(lz, 0, dim_z - 1);

    const std::size_t i = static_cast<std::size_t>(cx) +
                          static_cast<std::size_t>(cz) * static_cast<std::size_t>(dim_x) +
                          static_cast<std::size_t>(cy) * static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);

    const std::uint8_t s = skylight_[i];
    const std::uint8_t b = blocklight_[i];
    return (s > b) ? s : b;
}

std::uint8_t LightVolume::sample_skylight(int wx, int wy, int wz) const {
    if (!have_volume_) return 15u;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int lx = wx - origin_x_;
    const int ly = wy - origin_y_;
    const int lz = wz - origin_z_;

    // Fallback for blocks above volume top: assume open sky.
    if (ly >= dim_y) {
        return 15u;
    }

    const int cx = std::clamp(lx, 0, dim_x - 1);
    const int cy = std::clamp(ly, 0, dim_y - 1);
    const int cz = std::clamp(lz, 0, dim_z - 1);

    const std::size_t i = static_cast<std::size_t>(cx) +
                          static_cast<std::size_t>(cz) * static_cast<std::size_t>(dim_x) +
                          static_cast<std::size_t>(cy) * static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);
    return skylight_[i];
}

std::uint8_t LightVolume::sample_blocklight(int wx, int wy, int wz) const {
    if (!have_volume_) return 0u;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int lx = wx - origin_x_;
    const int ly = wy - origin_y_;
    const int lz = wz - origin_z_;

    const int cx = std::clamp(lx, 0, dim_x - 1);
    const int cy = std::clamp(ly, 0, dim_y - 1);
    const int cz = std::clamp(lz, 0, dim_z - 1);

    const std::size_t i = static_cast<std::size_t>(cx) +
                          static_cast<std::size_t>(cz) * static_cast<std::size_t>(dim_x) +
                          static_cast<std::size_t>(cy) * static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);
    return blocklight_[i];
}

float LightVolume::sample_combined01(int wx, int wy, int wz) const {
    const std::uint8_t l = sample_combined(wx, wy, wz);
    return static_cast<float>(l) / 15.0f;
}

float LightVolume::sample_skylight01(int wx, int wy, int wz) const {
    const std::uint8_t l = sample_skylight(wx, wy, wz);
    return static_cast<float>(l) / 15.0f;
}

float LightVolume::sample_blocklight01(int wx, int wy, int wz) const {
    const std::uint8_t l = sample_blocklight(wx, wy, wz);
    return static_cast<float>(l) / 15.0f;
}

} // namespace voxel
