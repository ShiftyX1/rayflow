package cmd

import (
	"fmt"
	"strings"
)

// =============================================================================
// game.toml
// =============================================================================

func generateGameToml(gameName, displayName, author string) string {
	if author == "" {
		author = "Developer"
	}
	return fmt.Sprintf(`# =============================================================================
# %s - Rayflow Game Project
# =============================================================================

[project]
name = "%s"
version = "0.1.0"
display_name = "%s"
description = "A game built on Rayflow Engine"
authors = ["%s"]

[engine]
version = ">=0.1.0"

[build]
cpp_standard = 20
client_name = "%s"
server_name = "%s_server"

[server]
tick_rate = 30
max_players = 16

[client]
window_width = 1280
window_height = 720
window_title = "%s"
target_fps = 60
vsync = true

[assets]
use_pak = false
pak_name = "assets.pak"
`, displayName, gameName, displayName, author, gameName, gameName, displayName)
}

// =============================================================================
// CMakeLists.txt - Standalone (uses SDK via find_package)
// =============================================================================

func generateCMakeListsStandalone(gameName, displayName, sdkDir string) string {
	// Convert backslashes to forward slashes for CMake compatibility
	sdkDirCMake := strings.ReplaceAll(sdkDir, "\\", "/")

	return fmt.Sprintf(`# =============================================================================
# %s - Built on Rayflow Engine
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(%s
    VERSION 0.1.0
    DESCRIPTION "%s"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Find Rayflow SDK
# =============================================================================
# SDK path set during project initialization by rayflow-cli
# To use a different SDK version, change RAYFLOW_SDK_DIR below

set(RAYFLOW_SDK_DIR "%s" CACHE PATH "Rayflow SDK location")
list(APPEND CMAKE_PREFIX_PATH "${RAYFLOW_SDK_DIR}/lib/cmake")

find_package(rayflow REQUIRED)

# =============================================================================
# Game Shared (protocol, types)
# =============================================================================
add_library(%s_shared STATIC
    src/shared/protocol.hpp
    src/shared/protocol.cpp
)

target_include_directories(%s_shared PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(%s_shared PUBLIC rayflow::core)

# =============================================================================
# Game Server (headless)
# =============================================================================
add_library(%s_server_lib STATIC
    src/server/game_server.hpp
    src/server/game_server.cpp
)

target_include_directories(%s_server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(%s_server_lib PUBLIC %s_shared)

# =============================================================================
# Game Client (rendering, UI)
# =============================================================================
add_library(%s_client_lib STATIC
    src/client/game_client.hpp
    src/client/game_client.cpp
)

target_include_directories(%s_client_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(%s_client_lib PUBLIC %s_shared rayflow::engine)

# =============================================================================
# Client Executable
# =============================================================================
add_executable(%s
    src/app/client_main.cpp
)

target_link_libraries(%s PRIVATE %s_client_lib %s_server_lib)

# Copy resources
add_custom_command(TARGET %s POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:%s>/resources
    COMMENT "Copying %s resources..."
)

# =============================================================================
# Dedicated Server Executable
# =============================================================================
add_executable(%s_server
    src/app/server_main.cpp
)

target_link_libraries(%s_server PRIVATE %s_server_lib rayflow::core)
`, displayName,
		gameName, displayName,
		sdkDirCMake,
		gameName, gameName, gameName,
		gameName, gameName, gameName, gameName,
		gameName, gameName, gameName, gameName,
		gameName, gameName, gameName, gameName,
		gameName, gameName, gameName,
		gameName, gameName, gameName)
}

// =============================================================================
// CMakeLists.txt - Engine Internal (for games inside engine repo)
// =============================================================================

func generateCMakeLists(gameName, displayName string) string {
	return fmt.Sprintf(`# =============================================================================
# %s - Built on Rayflow Engine
# =============================================================================
cmake_minimum_required(VERSION 3.21)

# =============================================================================
# Game Shared (protocol, types)
# =============================================================================
add_library(%s_shared STATIC
    src/shared/protocol.hpp
    src/shared/protocol.cpp
)

target_include_directories(%s_shared PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(%s_shared PUBLIC engine_core)

# =============================================================================
# Game Server (headless)
# =============================================================================
add_library(%s_server_lib STATIC
    src/server/game_server.hpp
    src/server/game_server.cpp
)

target_include_directories(%s_server_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(%s_server_lib PUBLIC %s_shared)

# =============================================================================
# Game Client (rendering, UI)
# =============================================================================
add_library(%s_client_lib STATIC
    src/client/game_client.hpp
    src/client/game_client.cpp
)

target_include_directories(%s_client_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(%s_client_lib PUBLIC %s_shared engine_client engine_ui engine_voxel)

# =============================================================================
# Client Executable
# =============================================================================
add_executable(%s
    src/app/client_main.cpp
)

target_link_libraries(%s PRIVATE %s_client_lib %s_server_lib)

# Copy resources
add_custom_command(TARGET %s POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:%s>/resources
    COMMENT "Copying %s resources..."
)

# =============================================================================
# Dedicated Server Executable
# =============================================================================
add_executable(%s_server
    src/app/server_main.cpp
)

target_link_libraries(%s_server PRIVATE %s_server_lib engine_core)
`, displayName,
		gameName, gameName, gameName,
		gameName, gameName, gameName, gameName,
		gameName, gameName, gameName, gameName,
		gameName, gameName, gameName, gameName,
		gameName, gameName, gameName,
		gameName, gameName, gameName)
}

// =============================================================================
// Protocol
// =============================================================================

func generateProtocolHpp(gameName string, standalone bool) string {
	ns := toNamespace(gameName)
	includePrefix := "engine"
	if standalone {
		includePrefix = "rayflow"
	}
	return fmt.Sprintf(`#pragma once

// =============================================================================
// %s - Protocol
// =============================================================================

#include <%s/core/byte_buffer.hpp>
#include <%s/core/types.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace %s::proto {

constexpr std::uint32_t kProtocolVersion = 1;

// =============================================================================
// Message Types
// =============================================================================

enum class MessageType : std::uint8_t {
    // Handshake
    ClientHello = 0,
    ServerHello = 1,
    
    // Input
    InputFrame = 2,
    
    // State
    StateSnapshot = 3,
};

// =============================================================================
// Client -> Server
// =============================================================================

struct ClientHello {
    std::uint32_t protocolVersion{kProtocolVersion};
    std::string playerName;
    
    void serialize(engine::ByteWriter& w) const;
    static ClientHello deserialize(engine::ByteReader& r);
};

struct InputFrame {
    std::uint32_t tick{0};
    float forward{0.0f};
    float right{0.0f};
    bool jump{false};
    bool sprint{false};
    float yaw{0.0f};
    float pitch{0.0f};
    
    void serialize(engine::ByteWriter& w) const;
    static InputFrame deserialize(engine::ByteReader& r);
};

// =============================================================================
// Server -> Client
// =============================================================================

struct ServerHello {
    std::uint32_t protocolVersion{kProtocolVersion};
    engine::PlayerId playerId{0};
    float tickRate{30.0f};
    
    void serialize(engine::ByteWriter& w) const;
    static ServerHello deserialize(engine::ByteReader& r);
};

struct PlayerState {
    engine::PlayerId id{0};
    float x{0.0f}, y{0.0f}, z{0.0f};
    float yaw{0.0f}, pitch{0.0f};
    
    void serialize(engine::ByteWriter& w) const;
    static PlayerState deserialize(engine::ByteReader& r);
};

struct StateSnapshot {
    std::uint32_t tick{0};
    std::vector<PlayerState> players;
    
    void serialize(engine::ByteWriter& w) const;
    static StateSnapshot deserialize(engine::ByteReader& r);
};

// =============================================================================
// Helpers
// =============================================================================

template<typename T>
std::vector<std::uint8_t> pack(MessageType type, const T& msg) {
    engine::ByteWriter w;
    w.write_u8(static_cast<std::uint8_t>(type));
    msg.serialize(w);
    return w.take();
}

} // namespace %s::proto
`, gameName, includePrefix, includePrefix, ns, ns)
}

func generateProtocolCpp(gameName string, standalone bool) string {
	ns := toNamespace(gameName)
	return fmt.Sprintf(`#include "protocol.hpp"

namespace %s::proto {

// =============================================================================
// ClientHello
// =============================================================================

void ClientHello::serialize(engine::ByteWriter& w) const {
    w.write_u32(protocolVersion);
    w.write_string(playerName);
}

ClientHello ClientHello::deserialize(engine::ByteReader& r) {
    ClientHello msg;
    msg.protocolVersion = r.read_u32();
    msg.playerName = r.read_string();
    return msg;
}

// =============================================================================
// InputFrame
// =============================================================================

void InputFrame::serialize(engine::ByteWriter& w) const {
    w.write_u32(tick);
    w.write_f32(forward);
    w.write_f32(right);
    w.write_u8((jump ? 1 : 0) | (sprint ? 2 : 0));
    w.write_f32(yaw);
    w.write_f32(pitch);
}

InputFrame InputFrame::deserialize(engine::ByteReader& r) {
    InputFrame msg;
    msg.tick = r.read_u32();
    msg.forward = r.read_f32();
    msg.right = r.read_f32();
    auto flags = r.read_u8();
    msg.jump = (flags & 1) != 0;
    msg.sprint = (flags & 2) != 0;
    msg.yaw = r.read_f32();
    msg.pitch = r.read_f32();
    return msg;
}

// =============================================================================
// ServerHello
// =============================================================================

void ServerHello::serialize(engine::ByteWriter& w) const {
    w.write_u32(protocolVersion);
    w.write_u32(playerId);
    w.write_f32(tickRate);
}

ServerHello ServerHello::deserialize(engine::ByteReader& r) {
    ServerHello msg;
    msg.protocolVersion = r.read_u32();
    msg.playerId = r.read_u32();
    msg.tickRate = r.read_f32();
    return msg;
}

// =============================================================================
// PlayerState
// =============================================================================

void PlayerState::serialize(engine::ByteWriter& w) const {
    w.write_u32(id);
    w.write_f32(x);
    w.write_f32(y);
    w.write_f32(z);
    w.write_f32(yaw);
    w.write_f32(pitch);
}

PlayerState PlayerState::deserialize(engine::ByteReader& r) {
    PlayerState state;
    state.id = r.read_u32();
    state.x = r.read_f32();
    state.y = r.read_f32();
    state.z = r.read_f32();
    state.yaw = r.read_f32();
    state.pitch = r.read_f32();
    return state;
}

// =============================================================================
// StateSnapshot
// =============================================================================

void StateSnapshot::serialize(engine::ByteWriter& w) const {
    w.write_u32(tick);
    w.write_u16(static_cast<std::uint16_t>(players.size()));
    for (const auto& p : players) {
        p.serialize(w);
    }
}

StateSnapshot StateSnapshot::deserialize(engine::ByteReader& r) {
    StateSnapshot msg;
    msg.tick = r.read_u32();
    auto count = r.read_u16();
    msg.players.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        msg.players.push_back(PlayerState::deserialize(r));
    }
    return msg;
}

} // namespace %s::proto
`, ns, ns)
}

// =============================================================================
// Server
// =============================================================================

func generateServerHpp(gameName, displayName string, standalone bool) string {
	ns := toNamespace(gameName)
	includePrefix := "engine"
	if standalone {
		includePrefix = "rayflow"
	}
	return fmt.Sprintf(`#pragma once

// =============================================================================
// %s - Game Server
// =============================================================================

#include <%s/core/game_interface.hpp>
#include "shared/protocol.hpp"

#include <string>
#include <unordered_map>

namespace %s {

struct ServerPlayer {
    engine::PlayerId id{0};
    std::string name;
    float x{0.0f}, y{0.0f}, z{0.0f};
    float vx{0.0f}, vy{0.0f}, vz{0.0f};
    float yaw{0.0f}, pitch{0.0f};
    proto::InputFrame lastInput;
};

class GameServer : public engine::IGameServer {
public:
    void on_init(engine::IEngineServices& engine) override;
    void on_shutdown() override;
    void on_tick(float dt) override;
    void on_player_connect(engine::PlayerId id) override;
    void on_player_disconnect(engine::PlayerId id) override;
    void on_player_message(engine::PlayerId id, std::span<const std::uint8_t> data) override;

private:
    void handle_client_hello(engine::PlayerId id, const proto::ClientHello& msg);
    void handle_input_frame(engine::PlayerId id, const proto::InputFrame& msg);
    void simulate_player(ServerPlayer& player, float dt);
    void broadcast_state();

    engine::IEngineServices* engine_{nullptr};
    std::unordered_map<engine::PlayerId, ServerPlayer> players_;
};

} // namespace %s
`, displayName, includePrefix, ns, ns)
}

func generateServerCpp(gameName, displayName string, standalone bool) string {
	ns := toNamespace(gameName)
	return fmt.Sprintf(`#include "game_server.hpp"

#include <cmath>
#include <sstream>

namespace %s {

void GameServer::on_init(engine::IEngineServices& engine) {
    engine_ = &engine;
    engine_->log_info("%s Server initialized");
}

void GameServer::on_shutdown() {
    engine_->log_info("%s Server shutting down");
    players_.clear();
}

void GameServer::on_tick(float dt) {
    for (auto& [id, player] : players_) {
        simulate_player(player, dt);
    }
    broadcast_state();
}

void GameServer::simulate_player(ServerPlayer& player, float dt) {
    const auto& input = player.lastInput;
    
    constexpr float kMoveSpeed = 5.0f;
    constexpr float kSprintMult = 1.5f;
    
    float speed = kMoveSpeed * (input.sprint ? kSprintMult : 1.0f);
    float rad = player.yaw * (3.14159f / 180.0f);
    
    player.x += (input.forward * std::cos(rad) - input.right * std::sin(rad)) * speed * dt;
    player.z += (input.forward * std::sin(rad) + input.right * std::cos(rad)) * speed * dt;
    player.yaw = input.yaw;
    player.pitch = input.pitch;
}

void GameServer::broadcast_state() {
    proto::StateSnapshot snapshot;
    snapshot.tick = engine_->current_tick();
    
    for (const auto& [id, player] : players_) {
        proto::PlayerState state;
        state.id = id;
        state.x = player.x;
        state.y = player.y;
        state.z = player.z;
        state.yaw = player.yaw;
        state.pitch = player.pitch;
        snapshot.players.push_back(state);
    }
    
    engine_->broadcast(proto::pack(proto::MessageType::StateSnapshot, snapshot));
}

void GameServer::on_player_connect(engine::PlayerId id) {
    std::ostringstream oss;
    oss << "Player " << id << " connecting...";
    engine_->log_info(oss.str());
    
    players_[id] = ServerPlayer{.id = id};
}

void GameServer::on_player_disconnect(engine::PlayerId id) {
    std::ostringstream oss;
    oss << "Player " << id << " disconnected";
    engine_->log_info(oss.str());
    players_.erase(id);
}

void GameServer::on_player_message(engine::PlayerId id, std::span<const std::uint8_t> data) {
    if (data.empty()) return;
    
    engine::ByteReader reader(data);
    auto msgType = static_cast<proto::MessageType>(reader.read_u8());
    
    switch (msgType) {
        case proto::MessageType::ClientHello:
            handle_client_hello(id, proto::ClientHello::deserialize(reader));
            break;
        case proto::MessageType::InputFrame:
            handle_input_frame(id, proto::InputFrame::deserialize(reader));
            break;
        default:
            break;
    }
}

void GameServer::handle_client_hello(engine::PlayerId id, const proto::ClientHello& msg) {
    if (msg.protocolVersion != proto::kProtocolVersion) {
        engine_->log_error("Protocol version mismatch");
        engine_->disconnect(id);
        return;
    }
    
    auto it = players_.find(id);
    if (it != players_.end()) {
        it->second.name = msg.playerName;
    }
    
    proto::ServerHello response;
    response.playerId = id;
    response.tickRate = engine_->tick_rate();
    engine_->send(id, proto::pack(proto::MessageType::ServerHello, response));
}

void GameServer::handle_input_frame(engine::PlayerId id, const proto::InputFrame& msg) {
    auto it = players_.find(id);
    if (it != players_.end()) {
        it->second.lastInput = msg;
    }
}

} // namespace %s
`, ns, displayName, displayName, ns)
}

// =============================================================================
// Client
// =============================================================================

func generateClientHpp(gameName, displayName string, standalone bool) string {
	ns := toNamespace(gameName)
	includePrefix := "engine"
	if standalone {
		includePrefix = "rayflow"
	}
	return fmt.Sprintf(`#pragma once

// =============================================================================
// %s - Game Client
// =============================================================================

#include <%s/core/game_interface.hpp>
#include "shared/protocol.hpp"

#include <string>
#include <unordered_map>

namespace %s {

struct ClientPlayer {
    engine::PlayerId id{0};
    float x{0.0f}, y{0.0f}, z{0.0f};
    float targetX{0.0f}, targetY{0.0f}, targetZ{0.0f};
    float yaw{0.0f}, pitch{0.0f};
};

class GameClient : public engine::IGameClient {
public:
    void set_player_name(const std::string& name) { playerName_ = name; }

    void on_init(engine::IClientServices& engine) override;
    void on_shutdown() override;
    void on_update(float dt) override;
    void on_render() override;
    void on_connected() override;
    void on_disconnected() override;
    void on_server_message(std::span<const std::uint8_t> data) override;

private:
    void handle_server_hello(const proto::ServerHello& msg);
    void handle_state_snapshot(const proto::StateSnapshot& msg);
    void capture_input();
    void send_input();
    void interpolate_players(float dt);

    engine::IClientServices* engine_{nullptr};
    std::string playerName_{"Player"};
    engine::PlayerId localPlayerId_{0};
    float serverTickRate_{30.0f};
    
    proto::InputFrame currentInput_;
    float inputSendTimer_{0.0f};
    float cameraYaw_{0.0f};
    float cameraPitch_{0.0f};
    
    std::unordered_map<engine::PlayerId, ClientPlayer> players_;
};

} // namespace %s
`, displayName, includePrefix, ns, ns)
}

func generateClientCpp(gameName, displayName string, standalone bool) string {
	ns := toNamespace(gameName)
	return fmt.Sprintf(`#include "game_client.hpp"

#include <raylib.h>
#include <cmath>

namespace %s {

void GameClient::on_init(engine::IClientServices& engine) {
    engine_ = &engine;
    engine_->log_info("%s Client initialized");
    DisableCursor();
}

void GameClient::on_shutdown() {
    engine_->log_info("%s Client shutting down");
    EnableCursor();
}

void GameClient::on_update(float dt) {
    capture_input();
    
    inputSendTimer_ += dt;
    if (inputSendTimer_ >= 1.0f / serverTickRate_) {
        send_input();
        inputSendTimer_ = 0.0f;
    }
    
    interpolate_players(dt);
}

void GameClient::capture_input() {
    currentInput_.forward = 0.0f;
    currentInput_.right = 0.0f;
    
    if (IsKeyDown(KEY_W)) currentInput_.forward += 1.0f;
    if (IsKeyDown(KEY_S)) currentInput_.forward -= 1.0f;
    if (IsKeyDown(KEY_D)) currentInput_.right += 1.0f;
    if (IsKeyDown(KEY_A)) currentInput_.right -= 1.0f;
    
    currentInput_.jump = IsKeyDown(KEY_SPACE);
    currentInput_.sprint = IsKeyDown(KEY_LEFT_SHIFT);
    
    Vector2 delta = GetMouseDelta();
    cameraYaw_ += delta.x * 0.1f;
    cameraPitch_ -= delta.y * 0.1f;
    cameraPitch_ = std::clamp(cameraPitch_, -89.0f, 89.0f);
    
    currentInput_.yaw = cameraYaw_;
    currentInput_.pitch = cameraPitch_;
}

void GameClient::send_input() {
    if (engine_->connection_state() != engine::ConnectionState::Connected) return;
    currentInput_.tick++;
    engine_->send(proto::pack(proto::MessageType::InputFrame, currentInput_));
}

void GameClient::interpolate_players(float dt) {
    for (auto& [id, p] : players_) {
        float t = std::min(1.0f, 15.0f * dt);
        p.x += (p.targetX - p.x) * t;
        p.y += (p.targetY - p.y) * t;
        p.z += (p.targetZ - p.z) * t;
    }
}

void GameClient::on_render() {
    ClearBackground(RAYWHITE);
    
    Vector3 camPos = {0, 2, 0};
    if (auto it = players_.find(localPlayerId_); it != players_.end()) {
        camPos = {it->second.x, it->second.y + 1.7f, it->second.z};
    }
    
    float yawRad = cameraYaw_ * DEG2RAD;
    float pitchRad = cameraPitch_ * DEG2RAD;
    Vector3 fwd = {
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad)
    };
    
    Camera3D camera = {0};
    camera.position = camPos;
    camera.target = {camPos.x + fwd.x, camPos.y + fwd.y, camPos.z + fwd.z};
    camera.up = {0, 1, 0};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    BeginMode3D(camera);
    DrawPlane({0, 0, 0}, {50, 50}, LIGHTGRAY);
    DrawGrid(50, 1.0f);
    
    for (const auto& [id, p] : players_) {
        if (id == localPlayerId_) continue;
        DrawCapsule({p.x, p.y + 0.1f, p.z}, {p.x, p.y + 1.7f, p.z}, 0.4f, 8, 8, BLUE);
    }
    EndMode3D();
    
    DrawText("%s", 10, 10, 20, DARKGRAY);
    DrawFPS(10, 40);
}

void GameClient::on_connected() {
    engine_->log_info("Connected, sending hello...");
    proto::ClientHello hello;
    hello.playerName = playerName_;
    engine_->send(proto::pack(proto::MessageType::ClientHello, hello));
}

void GameClient::on_disconnected() {
    engine_->log_info("Disconnected");
    players_.clear();
    localPlayerId_ = 0;
}

void GameClient::on_server_message(std::span<const std::uint8_t> data) {
    if (data.empty()) return;
    engine::ByteReader reader(data);
    auto msgType = static_cast<proto::MessageType>(reader.read_u8());
    
    switch (msgType) {
        case proto::MessageType::ServerHello:
            handle_server_hello(proto::ServerHello::deserialize(reader));
            break;
        case proto::MessageType::StateSnapshot:
            handle_state_snapshot(proto::StateSnapshot::deserialize(reader));
            break;
        default:
            break;
    }
}

void GameClient::handle_server_hello(const proto::ServerHello& msg) {
    localPlayerId_ = msg.playerId;
    serverTickRate_ = msg.tickRate;
}

void GameClient::handle_state_snapshot(const proto::StateSnapshot& msg) {
    for (const auto& s : msg.players) {
        auto& p = players_[s.id];
        p.id = s.id;
        if (p.targetX == 0 && p.targetY == 0 && p.targetZ == 0) {
            p.x = s.x; p.y = s.y; p.z = s.z;
        }
        p.targetX = s.x; p.targetY = s.y; p.targetZ = s.z;
        p.yaw = s.yaw; p.pitch = s.pitch;
    }
}

} // namespace %s
`, ns, displayName, displayName, displayName, ns)
}

// =============================================================================
// Entry Points
// =============================================================================

func generateClientMain(gameName, displayName string, standalone bool) string {
	ns := toNamespace(gameName)
	includePrefix := "engine"
	if standalone {
		includePrefix = "rayflow"
	}
	return fmt.Sprintf(`// =============================================================================
// %s - Client Entry Point
// =============================================================================

#include "client/game_client.hpp"
#include "server/game_server.hpp"

#include <%s/core/client_engine.hpp>
#include <%s/core/server_engine.hpp>
#include <%s/transport/local_transport.hpp>
#include <%s/transport/enet_client.hpp>

#include <cstring>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    std::string connectAddr;
    std::string playerName = "Player";
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
            connectAddr = argv[++i];
        } else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            playerName = argv[++i];
        }
    }
    
    %s::GameClient gameClient;
    gameClient.set_player_name(playerName);
    
    std::unique_ptr<%s::GameServer> gameServer;
    std::unique_ptr<engine::ServerEngine> serverEngine;
    std::thread serverThread;
    
    engine::ClientEngine::Config cfg;
    cfg.windowWidth = 1280;
    cfg.windowHeight = 720;
    cfg.windowTitle = "%s";
    cfg.targetFps = 60;
    
    engine::ClientEngine clientEngine(cfg);
    
    if (connectAddr.empty()) {
        // Singleplayer
        auto [client, server] = engine::transport::create_local_transport_pair();
        
        gameServer = std::make_unique<%s::GameServer>();
        serverEngine = std::make_unique<engine::ServerEngine>();
        serverEngine->set_transport(server);
        
        serverThread = std::thread([&]() { serverEngine->run(*gameServer); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        clientEngine.set_transport(client);
    } else {
        // Multiplayer
        std::string host = "localhost";
        std::uint16_t port = 7777;
        if (auto pos = connectAddr.find(':'); pos != std::string::npos) {
            host = connectAddr.substr(0, pos);
            port = static_cast<std::uint16_t>(std::stoi(connectAddr.substr(pos + 1)));
        } else {
            host = connectAddr;
        }
        
        auto enet = std::make_shared<engine::transport::ENetClientTransport>();
        if (!enet->connect(host, port)) {
            std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
            return 1;
        }
        clientEngine.set_transport(enet);
    }
    
    clientEngine.run(gameClient);
    
    if (serverEngine) serverEngine->stop();
    if (serverThread.joinable()) serverThread.join();
    
    return 0;
}
`, displayName, includePrefix, includePrefix, includePrefix, includePrefix, ns, ns, displayName, ns)
}

func generateServerMain(gameName, displayName string, standalone bool) string {
	ns := toNamespace(gameName)
	includePrefix := "engine"
	if standalone {
		includePrefix = "rayflow"
	}
	return fmt.Sprintf(`// =============================================================================
// %s - Dedicated Server
// =============================================================================

#include "server/game_server.hpp"

#include <%s/core/server_engine.hpp>
#include <%s/transport/enet_server.hpp>

#include <csignal>
#include <cstring>
#include <iostream>

static std::unique_ptr<engine::ServerEngine> g_server;

void signal_handler(int) {
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    std::uint16_t port = 7777;
    float tickRate = 30.0f;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--tickrate") == 0 && i + 1 < argc) {
            tickRate = std::stof(argv[++i]);
        }
    }
    
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "=== %s Server ===\n";
    std::cout << "Port: " << port << ", Tick Rate: " << tickRate << "\n\n";
    
    %s::GameServer game;
    
    engine::ServerEngine::Config cfg;
    cfg.tickRate = tickRate;
    
    auto transport = std::make_shared<engine::transport::ENetServerTransport>();
    
    g_server = std::make_unique<engine::ServerEngine>(cfg);
    g_server->set_transport(transport);
    g_server->run(game);
    
    return 0;
}
`, displayName, includePrefix, includePrefix, displayName, ns)
}

// =============================================================================
// README
// =============================================================================

func generateReadme(gameName, displayName string) string {
	return fmt.Sprintf(`# %s

A game built on [Rayflow Engine](https://github.com/rayflow/rayflow).

## Building

From engine root:
`+"```bash"+`
cmake --build --preset debug
`+"```"+`

## Running

### Singleplayer
`+"```bash"+`
./build/debug/%s
`+"```"+`

### Multiplayer

Server:
`+"```bash"+`
./build/debug/%s_server --port 7777
`+"```"+`

Client:
`+"```bash"+`
./build/debug/%s --connect localhost:7777 --name "Player"
`+"```"+`

## Controls

- **WASD** - Move
- **Mouse** - Look
- **Space** - Jump
- **Shift** - Sprint
`, displayName, gameName, gameName, gameName)
}

// =============================================================================
// Helpers
// =============================================================================

func toNamespace(gameName string) string {
	// Convert game_name to game_name (keep as-is for namespace)
	return strings.ReplaceAll(gameName, "-", "_")
}
