#pragma once

#include <raylib.h>
#include <cstdint>
#include <vector>

namespace voxel {
class World;
}

namespace renderer {

class LightingRaymarch {
public:
    struct Settings {
        int volume_x{64};
        int volume_y{96};
        int volume_z{64};

        int origin_step_voxels{4};
        float max_upload_hz{2.0f};

        float step_size_ws{0.5f};
        int max_steps{48};

        Vector3 sun_dir_ws{-0.35f, -1.0f, -0.25f};
        Vector3 sun_color{1.0f, 1.0f, 1.0f};
        Vector3 ambient_color{0.35f, 0.35f, 0.35f};
    };

    static LightingRaymarch& instance();

    bool init();
    void shutdown();

    void set_enabled(bool enabled);
    bool enabled() const { return enabled_; }

    void set_settings(const Settings& s);
    const Settings& settings() const { return settings_; }

    // Rebuilds/upload occupancy if needed (rate-limited).
    void update_volume_if_needed(const voxel::World& world, const Vector3& camera_pos_ws);

    // Call once per frame before drawing chunks.
    void apply_frame_uniforms();

    bool ready() const { return ready_; }
    Shader shader() const { return shader_; }

    // World-space origin of the current volume min corner (integer voxel aligned).
    Vector3 volume_origin_ws() const { return volume_origin_ws_; }

private:
    LightingRaymarch() = default;

    bool ensure_resources_();
    void rebuild_and_upload_volume_(const voxel::World& world);

    static int floor_div_(int a, int b);

    Settings settings_{};

    bool ready_{false};
    bool enabled_{false};

    Shader shader_{};
    int loc_enabled_{-1};
    int loc_sun_dir_{-1};
    int loc_sun_color_{-1};
    int loc_ambient_{-1};

    int loc_volume_origin_{-1};
    int loc_volume_size_{-1};
    int loc_occ_inv_size_{-1};

    int loc_step_size_{-1};
    int loc_max_steps_{-1};

    int loc_occ_tex_{-1};

    Texture2D occ_tex_{};
    int occ_w_{0};
    int occ_h_{0};

    std::vector<std::uint8_t> occ_rgba_{};

    bool have_volume_{false};
    int volume_origin_x_{0};
    int volume_origin_y_{0};
    int volume_origin_z_{0};
    Vector3 volume_origin_ws_{0.0f, 0.0f, 0.0f};

    double last_upload_time_{0.0};
};

} // namespace renderer
