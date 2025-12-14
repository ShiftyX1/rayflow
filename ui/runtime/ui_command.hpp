#pragma once

#include <variant>

namespace ui {

struct SetCameraSensitivity {
    float value{0.1f};
};

struct SetRaymarchLightingEnabled {
    bool enabled{false};
};

using UICommand = std::variant<SetCameraSensitivity, SetRaymarchLightingEnabled>;

} // namespace ui
