#pragma once

#include <variant>

namespace ui {

struct SetCameraSensitivity {
    float value{0.1f};
};

using UICommand = std::variant<SetCameraSensitivity>;

} // namespace ui
