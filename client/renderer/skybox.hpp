#pragma once

#include <raylib.h>

#include "../../shared/maps/rfmap_io.hpp"

#include <cstdint>
#include <string>

namespace renderer {

class Skybox {
public:
    static Skybox& instance();

    bool init();
    void shutdown();

    void set_kind(shared::maps::MapTemplate::SkyboxKind kind);
    shared::maps::MapTemplate::SkyboxKind kind() const { return kind_; }

    // Draw inside an active BeginMode3D/EndMode3D block.
    void draw(const Camera3D& camera);

    bool ready() const { return ready_; }

private:
    Skybox() = default;

    void ensure_cubemap_loaded_();
    const char* panorama_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind);
    const char* cubemap_path_for_kind_(shared::maps::MapTemplate::SkyboxKind kind);

    bool ready_{false};

    shared::maps::MapTemplate::SkyboxKind kind_{shared::maps::MapTemplate::SkyboxKind::Day};
    shared::maps::MapTemplate::SkyboxKind loaded_kind_{shared::maps::MapTemplate::SkyboxKind::None};

    std::string pano_path_;
    std::string cube_path_;

    Shader shader_{};

    Model model_{};
    bool has_model_{false};

    Texture2D cubemap_{}; // raylib uses Texture2D for cubemaps too
};

} // namespace renderer
