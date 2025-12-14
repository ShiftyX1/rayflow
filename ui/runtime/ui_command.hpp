#pragma once

#include <variant>

namespace ui {

struct SetCameraSensitivity {
    float value{0.1f};
};

struct SetRaymarchLightingEnabled {
    bool enabled{false};
};

struct SetRaymarchLightConfig {
    // 0..24
    float time_of_day_hours{12.0f};
    bool use_moon{false};

    float sun_intensity{1.0f};
    float ambient_intensity{0.35f};
};

using UICommand = std::variant<SetCameraSensitivity, SetRaymarchLightingEnabled, SetRaymarchLightConfig>;

} // namespace ui
