#include "lighting_raymarch.hpp"

#include "../voxel/world.hpp"
#include "../voxel/block.hpp"

#include <algorithm>
#include <cmath>

namespace renderer {

LightingRaymarch& LightingRaymarch::instance() {
    static LightingRaymarch g;
    return g;
}

bool LightingRaymarch::init() {
    if (ready_) return true;

    shader_ = LoadShader("shaders/voxel_raymarch.vs", "shaders/voxel_raymarch.fs");
    if (shader_.id == 0) {
        TraceLog(LOG_ERROR, "LightingRaymarch: failed to load shaders (shaders/voxel_raymarch.*)");
        ready_ = false;
        return false;
    }

    loc_enabled_ = GetShaderLocation(shader_, "u_enabled");
    loc_sun_dir_ = GetShaderLocation(shader_, "u_sunDirWS");
    loc_sun_color_ = GetShaderLocation(shader_, "u_sunColor");
    loc_ambient_ = GetShaderLocation(shader_, "u_ambientColor");

    loc_volume_origin_ = GetShaderLocation(shader_, "u_volumeOriginWS");
    loc_volume_size_ = GetShaderLocation(shader_, "u_volumeSize");
    loc_occ_inv_size_ = GetShaderLocation(shader_, "u_occInvSize");

    loc_step_size_ = GetShaderLocation(shader_, "u_stepSize");
    loc_max_steps_ = GetShaderLocation(shader_, "u_maxSteps");

    loc_occ_tex_ = GetShaderLocation(shader_, "u_occTex");

    ready_ = ensure_resources_();
    enabled_ = false;

    // Push initial uniforms once.
    apply_frame_uniforms();

    return ready_;
}

void LightingRaymarch::shutdown() {
    if (occ_tex_.id != 0) {
        UnloadTexture(occ_tex_);
        occ_tex_ = {};
    }

    if (shader_.id != 0) {
        UnloadShader(shader_);
        shader_ = {};
    }

    occ_rgba_.clear();
    occ_w_ = 0;
    occ_h_ = 0;

    ready_ = false;
    have_volume_ = false;
}

void LightingRaymarch::set_enabled(bool enabled) {
    enabled_ = enabled;
}

void LightingRaymarch::set_settings(const Settings& s) {
    settings_ = s;

    // Force resource/volume rebuild on next update.
    have_volume_ = false;
    last_upload_time_ = 0.0;

    // Recreate occupancy texture to match new dimensions.
    if (occ_tex_.id != 0) {
        UnloadTexture(occ_tex_);
        occ_tex_ = {};
    }

    ensure_resources_();
}

void LightingRaymarch::set_global_light_from_time_of_day(float time_of_day_hours, bool use_moon, float sun_intensity, float ambient_intensity) {
    // Clamp inputs to sane ranges (debug-driven).
    float h = time_of_day_hours;
    if (h < 0.0f) h = 0.0f;
    if (h > 24.0f) h = 24.0f;

    float sunI = sun_intensity;
    if (sunI < 0.0f) sunI = 0.0f;
    if (sunI > 10.0f) sunI = 10.0f;

    float ambI = ambient_intensity;
    if (ambI < 0.0f) ambI = 0.0f;
    if (ambI > 5.0f) ambI = 5.0f;

    // Very simple sky model for debug:
    // - Azimuth rotates over the day.
    // - Elevation follows a sine curve peaking at noon.
    // We keep a minimum elevation so shadows remain stable.

    constexpr float kPi = 3.14159265358979323846f;

    const float az = (h / 24.0f) * (2.0f * kPi);
    const float elev_sin = std::sin(((h - 6.0f) / 12.0f) * kPi); // ~0 at 6/18, 1 at 12
    float elev = elev_sin;

    if (use_moon) {
        // Moon roughly opposite the sun.
        elev = -elev_sin;
    }

    // Keep some light above horizon so ndl/shadows don't degenerate.
    const float y = std::max(0.10f, elev);
    const float x = std::cos(az);
    const float z = std::sin(az);

    Vector3 dir = { x, y, z };
    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.0001f) {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    } else {
        dir = { 0.0f, 1.0f, 0.0f };
    }

    settings_.sun_dir_ws = dir;

    if (use_moon) {
        // Slightly bluish moonlight.
        settings_.sun_color = { 0.65f * sunI, 0.70f * sunI, 0.85f * sunI };
    } else {
        settings_.sun_color = { 1.00f * sunI, 0.98f * sunI, 0.92f * sunI };
    }

    settings_.ambient_color = { ambI, ambI, ambI };
}

int LightingRaymarch::floor_div_(int a, int b) {
    // Floor division for possibly-negative integers.
    // b must be > 0.
    if (b <= 0) return 0;
    if (a >= 0) return a / b;
    return -static_cast<int>((static_cast<unsigned int>(-a) + static_cast<unsigned int>(b) - 1u) / static_cast<unsigned int>(b));
}

bool LightingRaymarch::ensure_resources_() {
    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int width = dim_x;
    const int height = dim_y * dim_z;

    if (occ_tex_.id != 0 && occ_w_ == width && occ_h_ == height) {
        return true;
    }

    if (occ_tex_.id != 0) {
        UnloadTexture(occ_tex_);
        occ_tex_ = {};
    }

    Image img = GenImageColor(width, height, BLACK);
    occ_tex_ = LoadTextureFromImage(img);
    UnloadImage(img);

    if (occ_tex_.id == 0) {
        TraceLog(LOG_ERROR, "LightingRaymarch: failed to create occupancy texture (%dx%d)", width, height);
        return false;
    }

    SetTextureFilter(occ_tex_, TEXTURE_FILTER_POINT);

    occ_w_ = width;
    occ_h_ = height;

    occ_rgba_.assign(static_cast<size_t>(occ_w_) * static_cast<size_t>(occ_h_) * 4u, 0u);

    return true;
}

void LightingRaymarch::update_volume_if_needed(const voxel::World& world, const Vector3& camera_pos_ws) {
    if (!ready_) return;
    if (!ensure_resources_()) return;

    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    const int step = std::max(1, settings_.origin_step_voxels);

    const int cam_x = static_cast<int>(std::floor(camera_pos_ws.x));
    const int cam_y = static_cast<int>(std::floor(camera_pos_ws.y));
    const int cam_z = static_cast<int>(std::floor(camera_pos_ws.z));

    int origin_x = cam_x - dim_x / 2;
    int origin_y = cam_y - dim_y / 2;
    int origin_z = cam_z - dim_z / 2;

    // Snap origin to a coarse voxel grid to reduce updates.
    origin_x = floor_div_(origin_x, step) * step;
    origin_y = floor_div_(origin_y, step) * step;
    origin_z = floor_div_(origin_z, step) * step;

    const bool origin_changed = (!have_volume_) || origin_x != volume_origin_x_ || origin_y != volume_origin_y_ || origin_z != volume_origin_z_;

    const double now = GetTime();
    const double min_dt = (settings_.max_upload_hz <= 0.0f) ? 0.0 : (1.0 / static_cast<double>(settings_.max_upload_hz));
    const bool rate_ok = (now - last_upload_time_) >= min_dt;

    if (!origin_changed || !rate_ok) {
        return;
    }

    volume_origin_x_ = origin_x;
    volume_origin_y_ = origin_y;
    volume_origin_z_ = origin_z;
    volume_origin_ws_ = { static_cast<float>(origin_x), static_cast<float>(origin_y), static_cast<float>(origin_z) };

    rebuild_and_upload_volume_(world);

    have_volume_ = true;
    last_upload_time_ = now;
}

void LightingRaymarch::rebuild_and_upload_volume_(const voxel::World& world) {
    const int dim_x = std::max(1, settings_.volume_x);
    const int dim_y = std::max(1, settings_.volume_y);
    const int dim_z = std::max(1, settings_.volume_z);

    // Packed 2D: width=dim_x, height=dim_y*dim_z, pixel (x, z*dim_y + y)
    // Each pixel is RGBA; we store occupancy in R.

    const size_t expected = static_cast<size_t>(occ_w_) * static_cast<size_t>(occ_h_) * 4u;
    if (occ_rgba_.size() != expected) {
        occ_rgba_.assign(expected, 0u);
    }

    for (int z = 0; z < dim_z; ++z) {
        for (int y = 0; y < dim_y; ++y) {
            const int row = z * dim_y + y;
            for (int x = 0; x < dim_x; ++x) {
                const int wx = volume_origin_x_ + x;
                const int wy = volume_origin_y_ + y;
                const int wz = volume_origin_z_ + z;

                const voxel::Block b = world.get_block(wx, wy, wz);
                const voxel::BlockType bt = static_cast<voxel::BlockType>(b);

                const bool occ = !voxel::is_transparent(bt);
                const std::uint8_t r = occ ? 255u : 0u;

                const size_t pixel = (static_cast<size_t>(row) * static_cast<size_t>(dim_x) + static_cast<size_t>(x));
                const size_t idx = pixel * 4u;
                occ_rgba_[idx + 0] = r;
                occ_rgba_[idx + 1] = 0u;
                occ_rgba_[idx + 2] = 0u;
                occ_rgba_[idx + 3] = 255u;
            }
        }
    }

    UpdateTexture(occ_tex_, occ_rgba_.data());
}

void LightingRaymarch::apply_frame_uniforms() {
    if (!ready_) return;

    const float enabled = enabled_ ? 1.0f : 0.0f;
    if (loc_enabled_ >= 0) SetShaderValue(shader_, loc_enabled_, &enabled, SHADER_UNIFORM_FLOAT);

    // Normalize sun dir.
    Vector3 d = settings_.sun_dir_ws;
    const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 0.0001f) {
        d.x /= len;
        d.y /= len;
        d.z /= len;
    }

    if (loc_sun_dir_ >= 0) SetShaderValue(shader_, loc_sun_dir_, &d, SHADER_UNIFORM_VEC3);
    if (loc_sun_color_ >= 0) SetShaderValue(shader_, loc_sun_color_, &settings_.sun_color, SHADER_UNIFORM_VEC3);
    if (loc_ambient_ >= 0) SetShaderValue(shader_, loc_ambient_, &settings_.ambient_color, SHADER_UNIFORM_VEC3);

    if (loc_volume_origin_ >= 0) SetShaderValue(shader_, loc_volume_origin_, &volume_origin_ws_, SHADER_UNIFORM_VEC3);

    Vector3 volume_size = { static_cast<float>(std::max(1, settings_.volume_x)), static_cast<float>(std::max(1, settings_.volume_y)), static_cast<float>(std::max(1, settings_.volume_z)) };
    if (loc_volume_size_ >= 0) SetShaderValue(shader_, loc_volume_size_, &volume_size, SHADER_UNIFORM_VEC3);

    Vector2 occ_inv = { (occ_w_ > 0) ? (1.0f / static_cast<float>(occ_w_)) : 0.0f, (occ_h_ > 0) ? (1.0f / static_cast<float>(occ_h_)) : 0.0f };
    if (loc_occ_inv_size_ >= 0) SetShaderValue(shader_, loc_occ_inv_size_, &occ_inv, SHADER_UNIFORM_VEC2);

    const float step = settings_.step_size_ws;
    if (loc_step_size_ >= 0) SetShaderValue(shader_, loc_step_size_, &step, SHADER_UNIFORM_FLOAT);

    const int max_steps = std::max(1, std::min(settings_.max_steps, 64));
    if (loc_max_steps_ >= 0) SetShaderValue(shader_, loc_max_steps_, &max_steps, SHADER_UNIFORM_INT);

    if (loc_occ_tex_ >= 0 && occ_tex_.id != 0) {
        SetShaderValueTexture(shader_, loc_occ_tex_, occ_tex_);
    }
}

} // namespace renderer
