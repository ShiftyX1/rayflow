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

    bool toggle_debug_ui{false}; // e.g. F1
};

struct UIFrameOutput {
    UIInputCapture capture{};
    std::vector<UICommand> commands{};
};

} // namespace ui
