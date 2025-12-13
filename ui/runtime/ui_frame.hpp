#pragma once

#include <vector>

#include "ui_command.hpp"

namespace ui {

struct UIInputCapture {
    bool wants_mouse{false};
    bool wants_keyboard{false};

    bool captured() const { return wants_mouse || wants_keyboard; }
};

struct UIFrameInput {
    float dt{0.0f};

    bool toggle_debug_ui{false};      // F1: interactive debug UI (windows/settings)
    bool toggle_debug_overlay{false}; // F2: overlay-only (informational)
};

struct UIFrameOutput {
    UIInputCapture capture{};
    std::vector<UICommand> commands{};
};

} // namespace ui
