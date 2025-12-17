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

    if ((!origin_changed && !force_rebuild) || !rate_ok) {
        return false;
    }

    origin_x_ = ox;
    origin_y_ = oy;
    origin_z_ = oz;
    volume_origin_ws_ = { static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(oz) };

    const std::size_t volume_size = static_cast<std::size_t>(dim_x) * static_cast<std::size_t>(dim_y) * static_cast<std::size_t>(dim_z);
    if (skylight_.size() != volume_size) skylight_.resize(volume_size);
    if (blocklight_.size() != volume_size) blocklight_.resize(volume_size);
    if (opaque_.size() != volume_size) opaque_.resize(volume_size);
    std::fill(skylight_.begin(), skylight_.end(), 0u);
    std::fill(blocklight_.begin(), blocklight_.end(), 0u);

    const auto t0 = std::chrono::steady_clock::now();
    rebuild_(world);
    const auto t1 = std::chrono::steady_clock::now();

    const float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
    const auto& prof = core::Config::instance().profiling();
    if (prof.enabled && prof.light_volume) {
        static double last_log_s = 0.0;
        const double now_s = GetTime();
        const bool interval_ok = prof.log_every_event || ((now_s - last_log_s) * 1000.0 >= static_cast<double>(std::max(0, prof.log_interval_ms)));
        if (ms >= prof.warn_light_volume_ms && interval_ok) {
            TraceLog(LOG_INFO, "[prof] light_volume rebuild: %.2f ms (dim=%dx%dx%d, force=%s)",
                     ms, dim_x, dim_y, dim_z, force_rebuild ? "true" : "false");
            last_log_s = now_s;
        }
    }

    have_volume_ = true;
    last_update_time_ = now;

    return true;
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
                const bool opaque = is_opaque_for_light(bt);
                const std::size_t i = idx(x, y, z);
                opaque_[i] = opaque ? 1u : 0u;

                if (!opaque && y == top_y) {
                    skylight_[i] = 15u;
                    q_sky_.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), 15u });
                }

                if (bt == BlockType::Light) {
                    blocklight_[i] = 15u;
                    q_blk_.push_back(QueueNode{ static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<std::uint16_t>(z), 15u });
                }
            }
        }
    }

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

    // BFS skylight
    for (std::size_t head = 0; head < q_sky_.size(); ++head) {
        const QueueNode n = q_sky_[head];

        if (n.level == 0) continue;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        // 6-neighborhood
        // Downward keeps same level, others attenuate.
        const std::uint8_t level_down = n.level;
        const std::uint8_t level_side = (n.level > 0) ? static_cast<std::uint8_t>(n.level - 1u) : 0u;

        try_push(q_sky_, skylight_, x, y - 1, z, level_down);
        try_push(q_sky_, skylight_, x + 1, y, z, level_side);
        try_push(q_sky_, skylight_, x - 1, y, z, level_side);
        try_push(q_sky_, skylight_, x, y, z + 1, level_side);
        try_push(q_sky_, skylight_, x, y, z - 1, level_side);
        try_push(q_sky_, skylight_, x, y + 1, z, level_side);
    }

    // BFS blocklight
    for (std::size_t head = 0; head < q_blk_.size(); ++head) {
        const QueueNode n = q_blk_[head];

        if (n.level <= 1) continue;

        const int x = static_cast<int>(n.x);
        const int y = static_cast<int>(n.y);
        const int z = static_cast<int>(n.z);

        const std::uint8_t next = static_cast<std::uint8_t>(n.level - 1u);

        try_push(q_blk_, blocklight_, x + 1, y, z, next);
        try_push(q_blk_, blocklight_, x - 1, y, z, next);
        try_push(q_blk_, blocklight_, x, y + 1, z, next);
        try_push(q_blk_, blocklight_, x, y - 1, z, next);
        try_push(q_blk_, blocklight_, x, y, z + 1, next);
        try_push(q_blk_, blocklight_, x, y, z - 1, next);
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
