#pragma once

#include <string>
#include <variant>
#include <cstdint>

namespace ui {

struct SetCameraSensitivity {
    float value{0.1f};
};

struct StartGame {};           // Start singleplayer
struct QuitGame {};
struct OpenSettings {};
struct CloseSettings {};
struct ResumeGame {};
struct OpenPauseMenu {};
struct ReturnToMainMenu {};

// Multiplayer
struct ShowConnectScreen {};
struct HideConnectScreen {};
struct ConnectToServer {
    std::string host{"localhost"};
    std::uint16_t port{7777};
};
struct DisconnectFromServer {};

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
    OpenPauseMenu,
    ReturnToMainMenu,
    ShowConnectScreen,
    HideConnectScreen,
    ConnectToServer,
    DisconnectFromServer,
    ButtonClicked
>;

} // namespace ui
