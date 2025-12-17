#pragma once

#include <cstdint>
#include <vector>

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

    bool ready() const { return have_volume_; }

    // Returns combined light (max of skylight and blocklight) in range [0..15].
    // If outside the current volume, returns 15 (neutral bright) to avoid black seams.
    std::uint8_t sample_combined(int wx, int wy, int wz) const;

    // Returns combined light in [0..1].
    float sample_combined01(int wx, int wy, int wz) const;

    Vector3 volume_origin_ws() const { return volume_origin_ws_; }

private:
    static int floor_div_(int a, int b);

    void rebuild_(const World& world);

    Settings settings_{};

    bool have_volume_{false};

    int origin_x_{0};
    int origin_y_{0};
    int origin_z_{0};
    Vector3 volume_origin_ws_{0.0f, 0.0f, 0.0f};

    double last_update_time_{0.0};

    std::vector<std::uint8_t> skylight_{};
    std::vector<std::uint8_t> blocklight_{};
};

} // namespace voxel
