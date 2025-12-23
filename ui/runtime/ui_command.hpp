#pragma once

#include <string>
#include <variant>

namespace ui {

struct SetCameraSensitivity {
    float value{0.1f};
};

struct StartGame {};
struct QuitGame {};
struct OpenSettings {};
struct CloseSettings {};
struct ResumeGame {};

struct ButtonClicked {
    std::string id{};
};

using UICommand = std::variant<
    SetCameraSensitivity,
    StartGame,
    QuitGame,
    OpenSettings,
    CloseSettings,
    ResumeGame,
    ButtonClicked
>;

} // namespace ui
