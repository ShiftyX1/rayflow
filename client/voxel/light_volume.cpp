#include "light_volume.hpp"

#include "world.hpp"
#include "block.hpp"

#include "../core/config.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace voxel {

namespace {

inline bool is_opaque_for_light(BlockType bt) {
    // Matches face-culling behavior: light passes through transparent blocks.
    // Leaves are treated as transparent per shared::voxel::util::is_transparent.
    return !is_transparent(bt);
}

} // namespace

void LightVolume::set_settings(const Settings& s) {
    settings_ = s;
    have_volume_ = false;
    last_update_time_ = 0.0;
    skylight_.clear();
    blocklight_.clear();
    opaque_.clear();
    q_sky_.clear();
    q_blk_.clear();

    building_ = false;
    build_phase_ = BuildPhase::None;
    skylight_build_.clear();
    blocklight_build_.clear();
    q_sky_head_ = 0;
    q_blk_head_ = 0;
    scan_x_ = scan_y_ = scan_z_ = 0;
    pending_force_rebuild_ = false;
    pending_origin_rebuild_ = false;
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

    // Track requests that arrive while a rebuild is already in progress.
    if (building_) {
        // If the build target origin differs from the newly desired origin, queue another rebuild
        // after the current one completes (prevents thrash / never-finishing builds).
        if (ox != build_origin_x_ || oy != build_origin_y_ || oz != build_origin_z_) {
            pending_origin_rebuild_ = true;
            pending_origin_x_ = ox;
            pending_origin_y_ = oy;
            pending_origin_z_ = oz;
        }
        if (force_rebuild) {
            pending_force_rebuild_ = true;
        }
    }

    auto start_build = [&](int bx, int by, int bz) {
        building_ = true;
        build_phase_ = BuildPhase::Scan;

        build_origin_x_ = bx;
        build_origin_y_ = by;
        build_origin_z_ = bz;
        build_volume_origin_ws_ = { static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bz) };

        scan_x_ = 0;
        scan_y_ = 0;
        scan_z_ = 0;
        q_sky_head_ = 0;
        q_blk_head_ = 0;
        q_sky_.clear();
        q_blk_.clear();

        const std::size_t volume_size = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_y) * static_cast<std::size_t>(dim_z);
        if (skylight_build_.size() != volume_size) skylight_build_.resize(volume_size);
        if (blocklight_build_.size() != volume_size) blocklight_build_.resize(volume_size);
        if (opaque_.size() != volume_size) opaque_.resize(volume_size);

        std::fill(skylight_build_.begin(), skylight_build_.end(), 0u);
        std::fill(blocklight_build_.begin(), blocklight_build_.end(), 0u);
        std::fill(opaque_.begin(), opaque_.end(), 0u);

        // Reserve queues based on worst-case (volume size). This is amortized.
        if (q_sky_.capacity() < volume_size) q_sky_.reserve(volume_size);
        if (q_blk_.capacity() < volume_size) q_blk_.reserve(volume_size);
    };

    const bool want_rebuild = origin_changed || force_rebuild || pending_force_rebuild_;

    // If not building, only start a new build when rate allows.
    if (!building_) {
        if (!want_rebuild || !rate_ok) {
            return false;
        }

        start_build(ox, oy, oz);
        pending_force_rebuild_ = false;
        pending_origin_rebuild_ = false;
    }

    // Time-sliced rebuild step.
    const float budget_ms = std::max(0.0f, settings_.rebuild_budget_ms);
    const auto t0 = std::chrono::steady_clock::now();
    const auto deadline = t0 + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double, std::milli>(static_cast<double>(budget_ms)));

    const std::size_t stride_z = static_cast<std::size_t>(dim_x);
    const std::size_t stride_y = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_z);
    const auto idx = [stride_z, stride_y](int x, int y, int z) -> std::size_t {
        return static_cast<std::size_t>(x) + static_cast<std::size_t>(z) * stride_z + static_cast<std::size_t>(y) * stride_y;
    };

    const int top_y = dim_y - 1;

    auto try_push = [idx, dim_x, dim_y, dim_z, this](
                        std::vector<QueueNode>& q,
                        std::vector<std::uint8_t>& channel,
                        int x, int y, int z,
                        std::uint8_t new_level) {
        if (new_level == 0) return;
        if (x < 0 || y < 0 || z < 0) return;
        if (x >= dim_x || y >= dim_y || z >= dim_z) return;

        const std::size_t i = idx(x, y, z);
        if (opaque_[i] != 0u) return;
        if (new_level <= channel[i]) return;

        channel[i] = new_level;
        q.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), new_level });
    };

    while (building_ && std::chrono::steady_clock::now() < deadline) {
        if (build_phase_ == BuildPhase::Scan) {
            // Incremental scan over the volume to fill opacity and seed sources.
            // Ordering: x -> z -> y.
            const int wx_base = build_origin_x_;
            const int wy_base = build_origin_y_;
            const int wz_base = build_origin_z_;

            // Process a chunk of voxels per loop iteration; time budget will stop us.
            // This reduces overhead of checking the clock every single voxel.
            constexpr int kScanBatch = 512;
            for (int n = 0; n < kScanBatch; ++n) {
                const std::size_t i = idx(scan_x_, scan_y_, scan_z_);

                const int wx = wx_base + scan_x_;
                const int wy = wy_base + scan_y_;
                const int wz = wz_base + scan_z_;

                const BlockType bt = static_cast<BlockType>(world.get_block(wx, wy, wz));
                const bool opaque = is_opaque_for_light(bt);
                opaque_[i] = opaque ? 1u : 0u;

                if (!opaque && scan_y_ == top_y) {
                    skylight_build_[i] = 15u;
                    q_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(scan_x_), static_cast<std::uint16_t>(scan_y_), static_cast<std::uint16_t>(scan_z_), 15u });
                }

                if (bt == BlockType::Light) {
                    blocklight_build_[i] = 15u;
                    q_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(scan_x_), static_cast<std::uint16_t>(scan_y_), static_cast<std::uint16_t>(scan_z_), 15u });
                }

                // Advance cursor.
                ++scan_x_;
                if (scan_x_ >= dim_x) {
                    scan_x_ = 0;
                    ++scan_z_;
                    if (scan_z_ >= dim_z) {
                        scan_z_ = 0;
                        ++scan_y_;
                        if (scan_y_ >= dim_y) {
                            // Scan complete.
                            build_phase_ = BuildPhase::Skylight;
                            q_sky_head_ = 0;
                            q_blk_head_ = 0;
                            break;
                        }
                    }
                }

                if (std::chrono::steady_clock::now() >= deadline) break;
            }
        } else if (build_phase_ == BuildPhase::Skylight) {
            constexpr int kBfsBatch = 1024;
            int processed = 0;
            while (q_sky_head_ < q_sky_.size() && processed < kBfsBatch) {
                const QueueNode n = q_sky_[q_sky_head_++];
                if (n.level == 0) {
                    ++processed;
                    continue;
                }

                const int x = static_cast<int>(n.x);
                const int y = static_cast<int>(n.y);
                const int z = static_cast<int>(n.z);

                const std::uint8_t level_down = n.level;
                const std::uint8_t level_side = (n.level > 0) ? static_cast<std::uint8_t>(n.level - 1u) : 0u;

                try_push(q_sky_, skylight_build_, x, y - 1, z, level_down);
                try_push(q_sky_, skylight_build_, x + 1, y, z, level_side);
                try_push(q_sky_, skylight_build_, x - 1, y, z, level_side);
                try_push(q_sky_, skylight_build_, x, y, z + 1, level_side);
                try_push(q_sky_, skylight_build_, x, y, z - 1, level_side);
                try_push(q_sky_, skylight_build_, x, y + 1, z, level_side);

                ++processed;
                if (std::chrono::steady_clock::now() >= deadline) break;
            }

            if (q_sky_head_ >= q_sky_.size()) {
                build_phase_ = BuildPhase::Blocklight;
            }
        } else if (build_phase_ == BuildPhase::Blocklight) {
            constexpr int kBfsBatch = 1024;
            int processed = 0;
            while (q_blk_head_ < q_blk_.size() && processed < kBfsBatch) {
                const QueueNode n = q_blk_[q_blk_head_++];
                if (n.level <= 1) {
                    ++processed;
                    continue;
                }

                const int x = static_cast<int>(n.x);
                const int y = static_cast<int>(n.y);
                const int z = static_cast<int>(n.z);

                const std::uint8_t next = static_cast<std::uint8_t>(n.level - 1u);
                try_push(q_blk_, blocklight_build_, x + 1, y, z, next);
                try_push(q_blk_, blocklight_build_, x - 1, y, z, next);
                try_push(q_blk_, blocklight_build_, x, y + 1, z, next);
                try_push(q_blk_, blocklight_build_, x, y - 1, z, next);
                try_push(q_blk_, blocklight_build_, x, y, z + 1, next);
                try_push(q_blk_, blocklight_build_, x, y, z - 1, next);

                ++processed;
                if (std::chrono::steady_clock::now() >= deadline) break;
            }

            if (q_blk_head_ >= q_blk_.size()) {
                // Build complete: swap in new buffers.
                skylight_.swap(skylight_build_);
                blocklight_.swap(blocklight_build_);
                origin_x_ = build_origin_x_;
                origin_y_ = build_origin_y_;
                origin_z_ = build_origin_z_;
                volume_origin_ws_ = build_volume_origin_ws_;

                have_volume_ = true;
                building_ = false;
                build_phase_ = BuildPhase::None;

                last_update_time_ = GetTime();

                const auto t1 = std::chrono::steady_clock::now();
                const float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
                const auto& prof = core::Config::instance().profiling();
                if (prof.enabled && prof.light_volume) {
                    static double last_log_s = 0.0;
                    const double now_s = GetTime();
                    const bool interval_ok = prof.log_every_event || ((now_s - last_log_s) * 1000.0 >= static_cast<double>(std::max(0, prof.log_interval_ms)));
                    if (ms >= prof.warn_light_volume_ms && interval_ok) {
                        TraceLog(LOG_INFO, "[prof] light_volume rebuild: %.2f ms (dim=%dx%dx%d, force=%s)",
                                 ms, dim_x, dim_y, dim_z, (force_rebuild || pending_force_rebuild_) ? "true" : "false");
                        last_log_s = now_s;
                    }
                }

                // If we accumulated requests during the build, trigger another build on the next frame.
                if (pending_origin_rebuild_) {
                    // Start from the queued origin.
                    const int nx = pending_origin_x_;
                    const int ny = pending_origin_y_;
                    const int nz = pending_origin_z_;
                    pending_origin_rebuild_ = false;
                    pending_force_rebuild_ = false; // force will be re-accumulated as needed

                    // Don't start immediately if rate limiting disallows; World will call again next frame.
                    const double now2 = GetTime();
                    const bool rate_ok2 = (min_dt <= 0.0) || ((now2 - last_update_time_) >= min_dt);
                    if (rate_ok2) {
                        start_build(nx, ny, nz);
                    } else {
                        // Keep a force flag so we rebuild once rate allows.
                        pending_force_rebuild_ = true;
                    }
                } else if (pending_force_rebuild_) {
                    // Rebuild at the current desired origin once rate allows.
                    const double now2 = GetTime();
                    const bool rate_ok2 = (min_dt <= 0.0) || ((now2 - last_update_time_) >= min_dt);
                    if (rate_ok2) {
                        pending_force_rebuild_ = false;
                        start_build(origin_x_, origin_y_, origin_z_);
                    }
                }

                return true;
            }
        } else {
            // Should not happen.
            building_ = false;
            build_phase_ = BuildPhase::None;
        }
    }

    return false;
}

void LightVolume::rebuild_(const World& /*world*/) {
    // Intentionally unused: rebuild is now time-sliced inside update_if_needed.
}

std::uint8_t LightVolume::sample_combined(int wx, int wy, int wz) const {
    if (!have_volume_) return 15u;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int lx = wx - origin_x_;
    const int ly = wy - origin_y_;
    const int lz = wz - origin_z_;

    const bool outside = (lx < 0 || ly < 0 || lz < 0 || lx >= dim_x || ly >= dim_y || lz >= dim_z);
    if (outside && building_) {
        // While the volume is being rebuilt incrementally, newly streamed-in chunks may
        // request lighting outside the last-complete volume. Clamping to the old edge can
        // bake those chunks as fully dark; prefer a bright fallback until the new volume swaps in.
        return 15u;
    }

    // Clamp out-of-volume samples to the edge to avoid:
    // - dark seams when the camera moves
    // - artificial skylight leaking into interiors via corner averaging
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

    const bool outside = (lx < 0 || ly < 0 || lz < 0 || lx >= dim_x || ly >= dim_y || lz >= dim_z);
    if (outside && building_) {
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

    const bool outside = (lx < 0 || ly < 0 || lz < 0 || lx >= dim_x || ly >= dim_y || lz >= dim_z);
    if (outside && building_) {
        return 0u;
    }

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
