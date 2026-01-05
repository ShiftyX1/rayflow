#include "bedwars_client.hpp"
#include "../shared/protocol/serialization.hpp"

// Engine subsystems are accessed through IClientServices
#include "engine/modules/voxel/client/world.hpp"
#include "engine/modules/voxel/client/block_interaction.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/systems/input_system.hpp"
#include "engine/ecs/systems/player_system.hpp"
#include "engine/renderer/skybox.hpp"
#include "engine/ui/runtime/ui_manager.hpp"
#include "engine/modules/voxel/shared/block.hpp"
#include "engine/maps/rfmap_io.hpp"
#include "engine/maps/runtime_paths.hpp"

#include <raylib.h>
#include <filesystem>
#include <raymath.h>

#include <cstdio>
#include <cmath>

namespace bedwars {

// ============================================================================
// Lifecycle
// ============================================================================

BedWarsClient::BedWarsClient() {
    // Initialize team colors
    teams_[0] = {proto::Teams::Red, true, RED};
    teams_[1] = {proto::Teams::Blue, true, BLUE};
    teams_[2] = {proto::Teams::Green, true, GREEN};
    teams_[3] = {proto::Teams::Yellow, true, YELLOW};
}

BedWarsClient::~BedWarsClient() = default;

void BedWarsClient::on_init(engine::IClientServices& engine) {
    engine_ = &engine;
    
    engine_->log(engine::LogLevel::Info, "BedWarsClient initialized");
    
    // Initialize ECS systems (same as rayflow)
    inputSystem_ = std::make_unique<ecs::InputSystem>();
    playerSystem_ = std::make_unique<ecs::PlayerSystem>();
    playerSystem_->set_client_replica_mode(true);  // Server-authoritative
    
    // Create player entity with all required components
    Vector3 spawnPos = {0.0f, 80.0f, 0.0f};
    playerEntity_ = ecs::PlayerSystem::create_player(registry_, spawnPos);
    
    // Start in main menu with cursor enabled
    gameScreen_ = ui::GameScreen::MainMenu;
    EnableCursor();
    
    engine_->log(engine::LogLevel::Info, "Starting in main menu");
}

void BedWarsClient::on_shutdown() {
    engine_->log(engine::LogLevel::Info, "BedWarsClient shutting down");
    inputSystem_.reset();
    playerSystem_.reset();
    registry_.clear();
    playerEntity_ = entt::null;
    EnableCursor();
}

// ============================================================================
// Update Loop
// ============================================================================

void BedWarsClient::on_update(float dt) {
    // Update UI view model
    update_ui_view_model();
    
    // Process UI (F1 = debug UI, F2 = debug overlay, same as rayflow)
    ui::UIFrameInput uiInput;
    uiInput.dt = dt;
    uiInput.toggle_pause = IsKeyPressed(KEY_ESCAPE);
    uiInput.toggle_debug_ui = IsKeyPressed(KEY_F1);
    uiInput.toggle_debug_overlay = IsKeyPressed(KEY_F2);
    
    ui::UIFrameOutput uiOutput = engine_->ui_manager().update(uiInput, uiViewModel_);
    uiCapturesInput_ = uiOutput.capture.captured();
    apply_ui_commands(uiOutput);
    
    // Handle cursor lock: only lock when playing in-game without UI capture
    bool shouldLockCursor = (gameScreen_ == ui::GameScreen::Playing) && 
                            (sessionState_ == SessionState::InGame) && 
                            !uiCapturesInput_;
    if (shouldLockCursor) {
        if (!IsCursorHidden()) {
            DisableCursor();
        }
    } else {
        if (IsCursorHidden()) {
            EnableCursor();
        }
    }
    
#ifdef NDEBUG
    // F3: Toggle BedWars-specific debug info (Release only)
    if (IsKeyPressed(KEY_F3)) {
        showDebug_ = !showDebug_;
    }
#endif
    
    // Update ECS systems when playing (same as rayflow)
    if (gameScreen_ == ui::GameScreen::Playing && sessionState_ == SessionState::InGame) {
        // Update input and player systems (same as rayflow)
        if (!uiCapturesInput_ && inputSystem_ && playerSystem_) {
            inputSystem_->update(registry_, dt);
            playerSystem_->update(registry_, dt);
        } else {
            clear_player_input();
        }
        
        // Send input to server
        send_input_frame();
        
        // Interpolate player position to server target each frame
        if (playerEntity_ != entt::null && registry_.all_of<ecs::Transform>(playerEntity_)) {
            auto& transform = registry_.get<ecs::Transform>(playerEntity_);
            
            // Frame-rate independent interpolation using exponential decay
            // speed=20 gives snappy movement (~95% convergence in ~0.15s)
            constexpr float kInterpSpeed = 20.0f;
            const float alpha = (dt <= 0.0f) ? 1.0f : (1.0f - std::exp(-kInterpSpeed * dt));
            
            transform.position.x += (localPlayer_.targetPx - transform.position.x) * alpha;
            transform.position.y += (localPlayer_.targetPy - transform.position.y) * alpha;
            transform.position.z += (localPlayer_.targetPz - transform.position.z) * alpha;
            
            // Also update localPlayer current position for consistency
            localPlayer_.px = transform.position.x;
            localPlayer_.py = transform.position.y;
            localPlayer_.pz = transform.position.z;
        }
        
        // Update world chunks around player
        if (playerEntity_ != entt::null) {
            auto& transform = registry_.get<ecs::Transform>(playerEntity_);
            
            try {
                auto& world = engine_->world();
                world.update(transform.position);
                
                // Update block interaction
                auto& blockInteraction = engine_->block_interaction();
                if (!uiCapturesInput_) {
                    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, playerEntity_);
                    Vector3 camDir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                    
                    auto& input = registry_.get<ecs::InputState>(playerEntity_);
                    auto& tool = registry_.get<ecs::ToolHolder>(playerEntity_);
                    
                    blockInteraction.update(world, camera.position, camDir, tool, 
                                            input.primary_action, input.secondary_action, dt);
                    
                    // Check for pending block operations
                    if (auto breakReq = blockInteraction.consume_break_request()) {
                        send_try_break_block(breakReq->x, breakReq->y, breakReq->z);
                    }
                    if (auto placeReq = blockInteraction.consume_place_request()) {
                        send_try_place_block(placeReq->x, placeReq->y, placeReq->z,
                                             static_cast<proto::BlockType>(placeReq->block_type),
                                             placeReq->hitY, placeReq->face);
                    }
                }
            } catch (const std::runtime_error&) {
                // World not initialized yet
            }
        }
    }
}

void BedWarsClient::clear_player_input() {
    if (playerEntity_ == entt::null || !registry_.all_of<ecs::InputState>(playerEntity_)) {
        return;
    }
    auto& input = registry_.get<ecs::InputState>(playerEntity_);
    input.move_input = {0.0f, 0.0f};
    input.look_input = {0.0f, 0.0f};
    input.jump_pressed = false;
    input.sprint_pressed = false;
    input.primary_action = false;
    input.secondary_action = false;
}

// ============================================================================
// Rendering
// ============================================================================

void BedWarsClient::on_render() {
    // Render based on game screen
    if (gameScreen_ == ui::GameScreen::MainMenu || 
        gameScreen_ == ui::GameScreen::ConnectMenu ||
        gameScreen_ == ui::GameScreen::Paused) {
        // Clear and render UI only
        ClearBackground(DARKGRAY);
        engine_->ui_manager().render(uiViewModel_);
        return;
    }
    
    if (gameScreen_ == ui::GameScreen::Connecting) {
        // Show connecting screen
        ClearBackground(BLACK);
        DrawText("Connecting to server...", 100, 100, 30, WHITE);
        
        if (sessionState_ == SessionState::WaitingServerHello) {
            DrawText("Waiting for ServerHello...", 100, 140, 20, GRAY);
        } else if (sessionState_ == SessionState::WaitingJoinAck) {
            DrawText("Joining match...", 100, 140, 20, GRAY);
        }
        
        if (!connectionError_.empty()) {
            DrawText(connectionError_.c_str(), 100, 200, 20, RED);
        }
        
        engine_->ui_manager().render(uiViewModel_);
        return;
    }
    
    // Playing - render 3D world
    if (playerEntity_ == entt::null) {
        ClearBackground(BLACK);
        engine_->ui_manager().render(uiViewModel_);
        return;
    }
    
    // Get camera from ECS (same as rayflow)
    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, playerEntity_);
    
    BeginMode3D(camera);
    
    // Draw skybox first (before world)
    renderer::Skybox::instance().draw(camera);
    
    render_world(camera);
    render_players();
    render_items();
    
    // Block interaction highlight
    try {
        auto& blockInteraction = engine_->block_interaction();
        blockInteraction.render_highlight(camera);
        blockInteraction.render_break_overlay(camera);
    } catch (const std::runtime_error&) {
        // Not initialized yet
    }
    
    EndMode3D();
    
    // 2D overlay
    // render_hud();
    
#ifdef NDEBUG
    // BedWars-specific debug info (F3 toggle, Release only)
    if (showDebug_) {
        render_debug_info();
    }
#endif
    
    // Crosshair (show when playing and UI not capturing input)
    if (!uiCapturesInput_) {
        voxel::BlockInteraction::render_crosshair(GetScreenWidth(), GetScreenHeight());
    }
    
    // Render UI (HUD, debug overlays handled by engine's UI manager)
    engine_->ui_manager().render(uiViewModel_);
}

void BedWarsClient::render_world(const Camera3D& camera) {
    try {
        engine_->world().render(camera);
    } catch (const std::runtime_error&) {
        // World not initialized
    }
}

void BedWarsClient::render_players() {
    // Render other players as simple colored cubes
    for (const auto& [id, player] : players_) {
        if (id == localPlayerId_) continue;  // Don't render self
        
        Color color = get_team_color(player.team);
        if (!player.alive) {
            color.a = 100;  // Semi-transparent when dead
        }
        
        Vector3 pos = {player.px, player.py + 0.9f, player.pz};  // Center of player
        DrawCube(pos, 0.6f, 1.8f, 0.6f, color);
        DrawCubeWires(pos, 0.6f, 1.8f, 0.6f, BLACK);
    }
}

void BedWarsClient::render_items() {
    // Render dropped items as small floating cubes
    for (const auto& [id, item] : items_) {
        Color color = GOLD;  // Different colors for different item types could be added
        
        Vector3 pos = {item.x, item.y + 0.2f, item.z};
        DrawCube(pos, 0.3f, 0.3f, 0.3f, color);
    }
}

void BedWarsClient::render_hud() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    
    // Health bar
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = sh - 40;
    
    float healthPercent = static_cast<float>(localPlayer_.hp) / static_cast<float>(localPlayer_.maxHp);
    
    DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
    DrawRectangle(barX, barY, static_cast<int>(barWidth * healthPercent), barHeight, RED);
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    
    // Health text
    char healthText[32];
    std::snprintf(healthText, sizeof(healthText), "%d / %d HP", localPlayer_.hp, localPlayer_.maxHp);
    DrawText(healthText, barX + barWidth + 10, barY + 2, 16, WHITE);
    
    // Team indicator
    Color teamColor = get_team_color(localPlayer_.team);
    DrawRectangle(sw - 120, 20, 100, 30, teamColor);
    
    const char* teamName = "None";
    switch (localPlayer_.team) {
        case proto::Teams::Red: teamName = "RED"; break;
        case proto::Teams::Blue: teamName = "BLUE"; break;
        case proto::Teams::Green: teamName = "GREEN"; break;
        case proto::Teams::Yellow: teamName = "YELLOW"; break;
        default: break;
    }
    DrawText(teamName, sw - 110, 25, 20, WHITE);
}

void BedWarsClient::render_debug_info() {
    char buf[256];
    int y = 10;
    
    std::snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
    DrawText(buf, 10, y, 16, GREEN);
    y += 20;
    
    // Get position and camera from ECS
    if (playerEntity_ != entt::null) {
        if (registry_.all_of<ecs::Transform>(playerEntity_)) {
            auto& pos = registry_.get<ecs::Transform>(playerEntity_).position;
            std::snprintf(buf, sizeof(buf), "Pos: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            DrawText(buf, 10, y, 16, WHITE);
            y += 20;
        }
        if (registry_.all_of<ecs::FirstPersonCamera>(playerEntity_)) {
            auto& cam = registry_.get<ecs::FirstPersonCamera>(playerEntity_);
            std::snprintf(buf, sizeof(buf), "Yaw: %.1f  Pitch: %.1f", cam.yaw, cam.pitch);
            DrawText(buf, 10, y, 16, WHITE);
            y += 20;
        }
    }
    
    std::snprintf(buf, sizeof(buf), "Seed: %u", worldSeed_);
    DrawText(buf, 10, y, 16, WHITE);
    y += 20;
    
    std::snprintf(buf, sizeof(buf), "Player ID: %u", localPlayerId_);
    DrawText(buf, 10, y, 16, WHITE);
    y += 20;
    
    // Network info
    std::snprintf(buf, sizeof(buf), "Ping: %u ms", engine_->ping_ms());
    DrawText(buf, 10, y, 16, YELLOW);
    y += 20;
    
    std::snprintf(buf, sizeof(buf), "Server Tick: %u", serverTick_);
    DrawText(buf, 10, y, 16, WHITE);
    y += 20;
    
    std::snprintf(buf, sizeof(buf), "Tick Rate: %u Hz", tickRate_);
    DrawText(buf, 10, y, 16, WHITE);
}

// ============================================================================
// Networking - Events
// ============================================================================

void BedWarsClient::on_connected() {
    engine_->log(engine::LogLevel::Info, "Connected to server");
    sessionState_ = SessionState::WaitingServerHello;
    
    // Send hello
    send_client_hello();
}

void BedWarsClient::on_disconnected() {
    engine_->log(engine::LogLevel::Info, "Disconnected from server");
    sessionState_ = SessionState::Disconnected;
    
    // Reset state
    localPlayerId_ = 0;
    players_.clear();
    items_.clear();
}

void BedWarsClient::on_server_message(std::span<const std::uint8_t> data) {
    auto msg = proto::deserialize(data);
    if (!msg) {
        engine_->log(engine::LogLevel::Warning, "Failed to deserialize server message");
        return;
    }
    
    handle_message(*msg);
}

// ============================================================================
// Message Handling
// ============================================================================

void BedWarsClient::handle_message(const proto::Message& msg) {
    std::visit([this](const auto& m) {
        using T = std::decay_t<decltype(m)>;
        
        if constexpr (std::is_same_v<T, proto::ServerHello>) {
            handle_server_hello(m);
        }
        else if constexpr (std::is_same_v<T, proto::JoinAck>) {
            handle_join_ack(m);
        }
        else if constexpr (std::is_same_v<T, proto::StateSnapshot>) {
            handle_state_snapshot(m);
        }
        else if constexpr (std::is_same_v<T, proto::ChunkData>) {
            handle_chunk_data(m);
        }
        else if constexpr (std::is_same_v<T, proto::BlockPlaced>) {
            handle_block_placed(m);
        }
        else if constexpr (std::is_same_v<T, proto::BlockBroken>) {
            handle_block_broken(m);
        }
        else if constexpr (std::is_same_v<T, proto::ActionRejected>) {
            handle_action_rejected(m);
        }
        else if constexpr (std::is_same_v<T, proto::TeamAssigned>) {
            handle_team_assigned(m);
        }
        else if constexpr (std::is_same_v<T, proto::HealthUpdate>) {
            handle_health_update(m);
        }
        else if constexpr (std::is_same_v<T, proto::PlayerDied>) {
            handle_player_died(m);
        }
        else if constexpr (std::is_same_v<T, proto::PlayerRespawned>) {
            handle_player_respawned(m);
        }
        else if constexpr (std::is_same_v<T, proto::BedDestroyed>) {
            handle_bed_destroyed(m);
        }
        else if constexpr (std::is_same_v<T, proto::ItemSpawned>) {
            handle_item_spawned(m);
        }
        else if constexpr (std::is_same_v<T, proto::ItemPickedUp>) {
            handle_item_picked_up(m);
        }
    }, msg);
}

void BedWarsClient::handle_server_hello(const proto::ServerHello& msg) {
    engine_->log(engine::LogLevel::Info, "Received ServerHello");
    
    tickRate_ = msg.tickRate;
    worldSeed_ = msg.worldSeed;
    hasMapTemplate_ = msg.hasMapTemplate;
    mapId_ = msg.mapId;
    mapVersion_ = msg.mapVersion;
    
    engine_->log(engine::LogLevel::Info, "ServerHello: hasMapTemplate=" + std::to_string(hasMapTemplate_) + 
                 " mapId=" + mapId_ + " mapVersion=" + std::to_string(mapVersion_));
    
    // Initialize world with seed through engine
    engine_->init_world(worldSeed_);
    
    // Try to load map template if server has one
    if (hasMapTemplate_ && !mapId_.empty() && mapVersion_ > 0) {
        // Look for map file locally: maps/<mapId>_v<version>.rfmap
        const std::string fileName = mapId_ + "_v" + std::to_string(mapVersion_) + ".rfmap";
        const std::filesystem::path path = shared::maps::runtime_maps_dir() / fileName;
        
        engine_->log(engine::LogLevel::Info, "Looking for map at: " + path.string());
        
        shared::maps::MapTemplate mapTemplate;
        std::string err;
        if (shared::maps::read_rfmap(path, &mapTemplate, &err)) {
            // Apply map template to world
            engine_->world().set_map_template(std::move(mapTemplate));
            
            // Apply visual settings (skybox, etc.)
            if (const auto* mt = engine_->world().map_template()) {
                renderer::Skybox::instance().set_kind(mt->visualSettings.skyboxKind);
            }
            
            engine_->log(engine::LogLevel::Info, "Loaded map template: " + fileName);
        } else {
            engine_->log(engine::LogLevel::Warning, "Map template not found locally: " + path.string() + " - " + err);
            // Client will still work, just won't have the base map
            // Server will send delta blocks anyway
        }
    } else {
        engine_->log(engine::LogLevel::Info, "No map template to load (hasMapTemplate=" + std::to_string(hasMapTemplate_) + ")");
    }
    
    sessionState_ = SessionState::WaitingJoinAck;
    
    // Send JoinMatch
    send_join_match();
}

void BedWarsClient::handle_join_ack(const proto::JoinAck& msg) {
    engine_->log(engine::LogLevel::Info, "Received JoinAck, player ID: " + std::to_string(msg.playerId));
    
    localPlayerId_ = msg.playerId;
    localPlayer_.playerId = msg.playerId;
    
    sessionState_ = SessionState::InGame;
    
    // Switch to playing mode
    gameScreen_ = ui::GameScreen::Playing;
    DisableCursor();
}

void BedWarsClient::handle_state_snapshot(const proto::StateSnapshot& msg) {
    // Track server tick
    serverTick_ = msg.serverTick;
    
    // Update local player position from server (authoritative)
    if (msg.playerId == localPlayerId_) {
        // Store target position for interpolation (done each frame in on_update)
        localPlayer_.targetPx = msg.px;
        localPlayer_.targetPy = msg.py;
        localPlayer_.targetPz = msg.pz;
        localPlayer_.vx = msg.vx;
        localPlayer_.vy = msg.vy;
        localPlayer_.vz = msg.vz;
        
        // Update velocity in ECS
        if (playerEntity_ != entt::null && registry_.all_of<ecs::Velocity>(playerEntity_)) {
            auto& vel = registry_.get<ecs::Velocity>(playerEntity_);
            vel.linear = {msg.vx, msg.vy, msg.vz};
        }
    } else {
        // Other player update
        auto& player = players_[msg.playerId];
        player.playerId = msg.playerId;
        player.px = msg.px;
        player.py = msg.py;
        player.pz = msg.pz;
        player.vx = msg.vx;
        player.vy = msg.vy;
        player.vz = msg.vz;
    }
}

void BedWarsClient::handle_chunk_data(const proto::ChunkData& msg) {
    try {
        engine_->world().apply_chunk_data(msg.chunkX, msg.chunkZ, msg.blocks);
    } catch (const std::runtime_error&) {
        // World not initialized
    }
}

void BedWarsClient::handle_block_placed(const proto::BlockPlaced& msg) {
    engine_->log(engine::LogLevel::Info, 
                 "BlockPlaced: " + std::to_string(msg.x) + "," + std::to_string(msg.y) + "," + std::to_string(msg.z) +
                 " type=" + std::to_string(static_cast<int>(msg.blockType)));
    try {
        engine_->world().set_block(msg.x, msg.y, msg.z, static_cast<voxel::Block>(msg.blockType));
    } catch (const std::runtime_error& e) {
        engine_->log(engine::LogLevel::Error, "Failed to set block: " + std::string(e.what()));
    }
}

void BedWarsClient::handle_block_broken(const proto::BlockBroken& msg) {
    engine_->log(engine::LogLevel::Info, 
                 "BlockBroken: " + std::to_string(msg.x) + "," + std::to_string(msg.y) + "," + std::to_string(msg.z));
    try {
        engine_->world().set_block(msg.x, msg.y, msg.z, static_cast<voxel::Block>(shared::voxel::BlockType::Air));
    } catch (const std::runtime_error& e) {
        engine_->log(engine::LogLevel::Error, "Failed to break block: " + std::string(e.what()));
    }
}

void BedWarsClient::handle_action_rejected(const proto::ActionRejected& msg) {
    engine_->log(engine::LogLevel::Warning, 
                 "Action rejected, seq=" + std::to_string(msg.seq) + 
                 " reason=" + std::to_string(static_cast<int>(msg.reason)));
    
    try {
        engine_->block_interaction().on_action_rejected();
    } catch (const std::runtime_error&) {
        // Not initialized
    }
}

void BedWarsClient::handle_team_assigned(const proto::TeamAssigned& msg) {
    engine_->log(engine::LogLevel::Info, 
                 "Team assigned: player " + std::to_string(msg.playerId) + 
                 " -> team " + std::to_string(msg.teamId));
    
    if (msg.playerId == localPlayerId_) {
        localPlayer_.team = msg.teamId;
    } else {
        players_[msg.playerId].team = msg.teamId;
    }
}

void BedWarsClient::handle_health_update(const proto::HealthUpdate& msg) {
    if (msg.playerId == localPlayerId_) {
        localPlayer_.hp = msg.hp;
        localPlayer_.maxHp = msg.maxHp;
    } else {
        players_[msg.playerId].hp = msg.hp;
        players_[msg.playerId].maxHp = msg.maxHp;
    }
}

void BedWarsClient::handle_player_died(const proto::PlayerDied& msg) {
    engine_->log(engine::LogLevel::Info, 
                 "Player died: " + std::to_string(msg.victimId) + 
                 " killed by " + std::to_string(msg.killerId) +
                 (msg.isFinalKill ? " (FINAL KILL)" : ""));
    
    if (msg.victimId == localPlayerId_) {
        localPlayer_.alive = false;
    } else {
        players_[msg.victimId].alive = false;
    }
}

void BedWarsClient::handle_player_respawned(const proto::PlayerRespawned& msg) {
    engine_->log(engine::LogLevel::Info, "Player respawned: " + std::to_string(msg.playerId));
    
    if (msg.playerId == localPlayerId_) {
        localPlayer_.alive = true;
        localPlayer_.px = msg.x;
        localPlayer_.py = msg.y;
        localPlayer_.pz = msg.z;
    } else {
        auto& player = players_[msg.playerId];
        player.alive = true;
        player.px = msg.x;
        player.py = msg.y;
        player.pz = msg.z;
    }
}

void BedWarsClient::handle_bed_destroyed(const proto::BedDestroyed& msg) {
    engine_->log(engine::LogLevel::Info, 
                 "Bed destroyed: team " + std::to_string(msg.teamId) + 
                 " by player " + std::to_string(msg.destroyerId));
    
    if (msg.teamId > 0 && msg.teamId <= teams_.size()) {
        teams_[msg.teamId - 1].hasBed = false;
    }
}

void BedWarsClient::handle_item_spawned(const proto::ItemSpawned& msg) {
    ClientItemState item;
    item.entityId = msg.entityId;
    item.type = msg.itemType;
    item.x = msg.x;
    item.y = msg.y;
    item.z = msg.z;
    item.count = msg.count;
    
    items_[msg.entityId] = item;
}

void BedWarsClient::handle_item_picked_up(const proto::ItemPickedUp& msg) {
    items_.erase(msg.entityId);
}

// ============================================================================
// Message Sending
// ============================================================================

void BedWarsClient::send_client_hello() {
    proto::ClientHello msg;
    msg.version = proto::kProtocolVersion;
    msg.clientName = playerName_;
    
    send_message(msg);
}

void BedWarsClient::send_join_match() {
    proto::JoinMatch msg;
    send_message(msg);
}

void BedWarsClient::send_input_frame() {
    if (playerEntity_ == entt::null) return;
    
    auto& input = registry_.get<ecs::InputState>(playerEntity_);
    auto& fpsCamera = registry_.get<ecs::FirstPersonCamera>(playerEntity_);
    
    proto::InputFrame msg;
    msg.seq = inputSeq_++;
    // Send input from ECS components (same as rayflow)
    msg.moveX = uiCapturesInput_ ? 0.0f : input.move_input.x;
    msg.moveY = uiCapturesInput_ ? 0.0f : input.move_input.y;
    msg.yaw = fpsCamera.yaw;
    msg.pitch = fpsCamera.pitch;
    msg.jump = uiCapturesInput_ ? false : input.jump_pressed;
    msg.sprint = uiCapturesInput_ ? false : input.sprint_pressed;
    msg.camUp = false;
    msg.camDown = false;
    
    send_message(msg);
}

void BedWarsClient::send_try_break_block(int x, int y, int z) {
    proto::TryBreakBlock msg;
    msg.seq = actionSeq_++;
    msg.x = x;
    msg.y = y;
    msg.z = z;
    
    send_message(msg);
}

void BedWarsClient::send_try_place_block(int x, int y, int z, proto::BlockType type, float hitY, std::uint8_t face) {
    proto::TryPlaceBlock msg;
    msg.seq = actionSeq_++;
    msg.x = x;
    msg.y = y;
    msg.z = z;
    msg.blockType = type;
    msg.hitY = hitY;
    msg.face = face;
    
    send_message(msg);
}

template<typename T>
void BedWarsClient::send_message(const T& msg) {
    auto data = proto::serialize(proto::Message{msg});
    engine_->send(data);
}

// ============================================================================
// UI
// ============================================================================

void BedWarsClient::update_ui_view_model() {
    uiViewModel_.screen_width = engine_->window_width();
    uiViewModel_.screen_height = engine_->window_height();
    uiViewModel_.fps = GetFPS();
    uiViewModel_.game_screen = gameScreen_;
    
    // Player info from ECS (same as rayflow)
    if (playerEntity_ != entt::null) {
        if (registry_.all_of<ecs::Transform>(playerEntity_)) {
            uiViewModel_.player.position = registry_.get<ecs::Transform>(playerEntity_).position;
        }
        if (registry_.all_of<ecs::Velocity>(playerEntity_)) {
            uiViewModel_.player.velocity = registry_.get<ecs::Velocity>(playerEntity_).linear;
        }
        if (registry_.all_of<ecs::FirstPersonCamera>(playerEntity_)) {
            auto& cam = registry_.get<ecs::FirstPersonCamera>(playerEntity_);
            uiViewModel_.player.yaw = cam.yaw;
            uiViewModel_.player.pitch = cam.pitch;
        }
        if (registry_.all_of<ecs::PlayerController>(playerEntity_)) {
            auto& pc = registry_.get<ecs::PlayerController>(playerEntity_);
            uiViewModel_.player.on_ground = pc.on_ground;
            uiViewModel_.player.sprinting = pc.is_sprinting;
            uiViewModel_.player.camera_sensitivity = pc.camera_sensitivity;
        }
    }
    
    // Health from network state
    uiViewModel_.player.health = localPlayer_.hp;
    uiViewModel_.player.max_health = localPlayer_.maxHp;
    
    // Network info
    uiViewModel_.net.is_connecting = (sessionState_ == SessionState::Connecting || 
                                       sessionState_ == SessionState::WaitingServerHello ||
                                       sessionState_ == SessionState::WaitingJoinAck);
    uiViewModel_.net.has_server_hello = (sessionState_ >= SessionState::WaitingJoinAck);
    uiViewModel_.net.has_join_ack = (sessionState_ == SessionState::InGame);
    uiViewModel_.net.tick_rate = tickRate_;
    uiViewModel_.net.world_seed = worldSeed_;
    uiViewModel_.net.player_id = localPlayerId_;
    uiViewModel_.net.connection_error = connectionError_;
    uiViewModel_.net.connection_failed = !connectionError_.empty();
    uiViewModel_.net.ping_ms = engine_->ping_ms();
    uiViewModel_.net.has_snapshot = (sessionState_ == SessionState::InGame);
    uiViewModel_.net.server_tick = serverTick_;
    // Remote connection is determined by having non-zero ping (LocalTransport always returns 0)
    uiViewModel_.net.is_remote_connection = (engine_->ping_ms() > 0);
}

void BedWarsClient::apply_ui_commands(const ui::UIFrameOutput& out) {
    for (const auto& cmd : out.commands) {
        if (std::get_if<ui::StartGame>(&cmd)) {
            // Start singleplayer
            if (onStartSingleplayer_) {
                engine_->log(engine::LogLevel::Info, "Starting singleplayer...");
                gameScreen_ = ui::GameScreen::Connecting;
                onStartSingleplayer_();
            }
        }
        else if (std::get_if<ui::QuitGame>(&cmd)) {
            // Request engine shutdown
            engine_->log(engine::LogLevel::Info, "Quit requested");
            // We can't directly stop engine from here, but user can close window
        }
        else if (std::get_if<ui::ShowConnectScreen>(&cmd)) {
            gameScreen_ = ui::GameScreen::ConnectMenu;
            connectionError_.clear();
        }
        else if (std::get_if<ui::HideConnectScreen>(&cmd)) {
            gameScreen_ = ui::GameScreen::MainMenu;
            connectionError_.clear();
        }
        else if (const auto* c = std::get_if<ui::ConnectToServer>(&cmd)) {
            // Connect to multiplayer server
            if (onConnectMultiplayer_) {
                engine_->log(engine::LogLevel::Info, "Connecting to " + c->host + ":" + std::to_string(c->port));
                gameScreen_ = ui::GameScreen::Connecting;
                connectionError_.clear();
                
                if (!onConnectMultiplayer_(c->host, c->port)) {
                    connectionError_ = "Failed to connect";
                    gameScreen_ = ui::GameScreen::ConnectMenu;
                }
            }
        }
        else if (std::get_if<ui::DisconnectFromServer>(&cmd)) {
            if (onDisconnect_) {
                onDisconnect_();
            }
            gameScreen_ = ui::GameScreen::MainMenu;
            sessionState_ = SessionState::Disconnected;
        }
        else if (std::get_if<ui::ResumeGame>(&cmd)) {
            if (sessionState_ == SessionState::InGame) {
                gameScreen_ = ui::GameScreen::Playing;
                DisableCursor();
            }
        }
        else if (std::get_if<ui::OpenPauseMenu>(&cmd)) {
            if (sessionState_ == SessionState::InGame) {
                gameScreen_ = ui::GameScreen::Paused;
                EnableCursor();
            }
        }
        else if (std::get_if<ui::ReturnToMainMenu>(&cmd)) {
            if (onDisconnect_) {
                onDisconnect_();
            }
            gameScreen_ = ui::GameScreen::MainMenu;
            sessionState_ = SessionState::Disconnected;
            EnableCursor();
        }
    }
}

// ============================================================================
// Helpers
// ============================================================================

Color BedWarsClient::get_team_color(proto::TeamId team) const {
    switch (team) {
        case proto::Teams::Red: return RED;
        case proto::Teams::Blue: return BLUE;
        case proto::Teams::Green: return GREEN;
        case proto::Teams::Yellow: return YELLOW;
        default: return WHITE;
    }
}

} // namespace bedwars
