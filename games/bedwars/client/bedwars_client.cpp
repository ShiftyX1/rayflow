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

#include "engine/client/core/input.hpp"
#include "engine/core/math_types.hpp"
#include "engine/core/key_codes.hpp"
#include "engine/core/logging.hpp"
#include "engine/renderer/batch_2d.hpp"
#include "engine/renderer/gl_font.hpp"
#include "engine/client/core/window.hpp"
#include <filesystem>

#include <cstdio>
#include <cmath>

// NOTE(migration): GetFPS placeholder — will be replaced in Phase 2 (renderer frame stats).
static int GetFPS() { return 0; }

namespace bedwars {

// ============================================================================
// Lifecycle
// ============================================================================

BedWarsClient::BedWarsClient() {
    // Initialize team colors
    teams_[0] = {proto::Teams::Red, true, rf::Color::Red()};
    teams_[1] = {proto::Teams::Blue, true, rf::Color::Blue()};
    teams_[2] = {proto::Teams::Green, true, rf::Color::Green()};
    teams_[3] = {proto::Teams::Yellow, true, rf::Color::Yellow()};
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
    rf::Vec3 spawnPos = {0.0f, 80.0f, 0.0f};
    playerEntity_ = ecs::PlayerSystem::create_player(registry_, spawnPos);
    
    // Start in main menu with cursor enabled
    gameScreen_ = ui::GameScreen::MainMenu;
    rf::Input::instance().setCursorMode(rf::CursorMode::Normal);
    
    engine_->log(engine::LogLevel::Info, "Starting in main menu");
}

void BedWarsClient::on_shutdown() {
    engine_->log(engine::LogLevel::Info, "BedWarsClient shutting down");
    inputSystem_.reset();
    playerSystem_.reset();
    registry_.clear();
    playerEntity_ = entt::null;
    rf::Input::instance().setCursorMode(rf::CursorMode::Normal);
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
    auto& rfInput = rf::Input::instance();
    uiInput.toggle_pause = rfInput.isKeyPressed(KEY_ESCAPE);
    uiInput.toggle_debug_ui = rfInput.isKeyPressed(KEY_F1);
    uiInput.toggle_debug_overlay = rfInput.isKeyPressed(KEY_F2);
    
    ui::UIFrameOutput uiOutput = engine_->ui_manager().update(uiInput, uiViewModel_);
    uiCapturesInput_ = uiOutput.capture.captured();
    apply_ui_commands(uiOutput);
    
    // Handle cursor lock: only lock when playing in-game without UI capture
    bool shouldLockCursor = (gameScreen_ == ui::GameScreen::Playing) && 
                            (sessionState_ == SessionState::InGame) && 
                            !uiCapturesInput_;
    if (shouldLockCursor) {
        if (!rfInput.isCursorHidden()) {
            rfInput.setCursorMode(rf::CursorMode::Disabled);
        }
    } else {
        if (rfInput.isCursorHidden()) {
            rfInput.setCursorMode(rf::CursorMode::Normal);
        }
    }
    
#ifdef NDEBUG
    // F3: Toggle BedWars-specific debug info (Release only)
    if (rfInput.isKeyPressed(KEY_F3)) {
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

                update_lights();
                
                // Update block interaction
                auto& blockInteraction = engine_->block_interaction();
                if (!uiCapturesInput_) {
                    ecs::Camera3D camera = ecs::PlayerSystem::get_camera(registry_, playerEntity_);
                    rf::Vec3 camDiff = camera.target - camera.position;
                    float camLen = std::sqrt(camDiff.x*camDiff.x + camDiff.y*camDiff.y + camDiff.z*camDiff.z);
                    rf::Vec3 camDir = (camLen > 0.0001f) ? camDiff / camLen : rf::Vec3{0,0,1};
                    
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
    auto& win = rf::Window::instance();
    int sw = win.width();
    int sh = win.height();
    auto& batch = rf::Batch2D::instance();

    // Render based on game screen
    if (gameScreen_ == ui::GameScreen::MainMenu || 
        gameScreen_ == ui::GameScreen::ConnectMenu ||
        gameScreen_ == ui::GameScreen::Paused) {
        batch.begin(sw, sh);
        batch.end();
        engine_->ui_manager().render(uiViewModel_);
        return;
    }
    
    if (gameScreen_ == ui::GameScreen::Connecting) {
        batch.begin(sw, sh);
        batch.drawText("Connecting to server...", 100, 100, 30, rf::Color::White());

        if (sessionState_ == SessionState::WaitingServerHello) {
            batch.drawText("Waiting for ServerHello...", 100, 140, 20, rf::Color{130,130,130,255});
        } else if (sessionState_ == SessionState::WaitingJoinAck) {
            batch.drawText("Joining match...", 100, 140, 20, rf::Color{130,130,130,255});
        }

        if (!connectionError_.empty()) {
            batch.drawText(connectionError_, 100, 200, 20, rf::Color::Red());
        }
        batch.end();
        
        engine_->ui_manager().render(uiViewModel_);
        return;
    }
    
    // Playing - render 3D world
    if (playerEntity_ == entt::null) {
        engine_->ui_manager().render(uiViewModel_);
        return;
    }
    
    // // Get camera from ECS (same as rayflow)
    // Camera3D camera = ecs::PlayerSystem::get_camera(registry_, playerEntity_);
    //
    // BeginMode3D(camera);
    //
    // // Draw skybox first (before world)
    // renderer::Skybox::instance().draw(camera);
    //
    // render_world(camera);
    // render_players();
    // render_items();
    //
    // // Block interaction highlight
    // try {
    //     auto& blockInteraction = engine_->block_interaction();
    //     blockInteraction.render_highlight(camera);
    //     blockInteraction.render_break_overlay(camera);
    // } catch (const std::runtime_error&) {
    //     // Not initialized yet
    // }
    //
    // EndMode3D();
    
    // 2D overlay
    batch.begin(sw, sh);
    render_hud();
    
#ifdef NDEBUG
    // BedWars-specific debug info (F3 toggle, Release only)
    if (showDebug_) {
        render_debug_info();
    }
#endif
    
    // Crosshair (show when playing and UI not capturing input)
    if (!uiCapturesInput_) {
        // Simple crosshair
        int cx = sw / 2, cy = sh / 2;
        batch.drawRect(static_cast<float>(cx - 10), static_cast<float>(cy - 1), 20.0f, 2.0f, rf::Color::White());
        batch.drawRect(static_cast<float>(cx - 1), static_cast<float>(cy - 10), 2.0f, 20.0f, rf::Color::White());
    }
    
    batch.end();
    
    // Render UI (HUD, debug overlays handled by engine's UI manager)
    engine_->ui_manager().render(uiViewModel_);
}

#if 0 // TODO(migration): Phase 2 — restore render_world
void BedWarsClient::render_world(const ecs::Camera3D& camera) {
    (void)camera;
}
#endif

void BedWarsClient::render_players() {
    // TODO(Phase 2): Restore player rendering with GLFW+OpenGL backend
    // Render other players as simple colored cubes
    for (const auto& [id, player] : players_) {
        if (id == localPlayerId_) continue;  // Don't render self
        
        rf::Color color = get_team_color(player.team);
        if (!player.alive) {
            color.a = 100;  // Semi-transparent when dead
        }
        
        rf::Vec3 pos = {player.px, player.py + 0.9f, player.pz};  // Center of player
        // DrawCube(pos, 0.6f, 1.8f, 0.6f, color);      // TODO(Phase 2)
        // DrawCubeWires(pos, 0.6f, 1.8f, 0.6f, BLACK);  // TODO(Phase 2)
        (void)pos;
        (void)color;
    }
}

void BedWarsClient::render_items() {
    // TODO(Phase 2): Restore item rendering with GLFW+OpenGL backend
    // Render dropped items as small floating cubes
    for (const auto& [id, item] : items_) {
        rf::Color color = rf::Color{255, 203, 0, 255};  // GOLD
        
        rf::Vec3 pos = {item.x, item.y + 0.2f, item.z};
        // DrawCube(pos, 0.3f, 0.3f, 0.3f, color);  // TODO(Phase 2)
        (void)pos;
        (void)color;
    }
}

void BedWarsClient::render_hud() {
    auto& win = rf::Window::instance();
    int sw = win.width();
    int sh = win.height();
    auto& batch = rf::Batch2D::instance();

    // Health bar
    int barWidth = 200;
    int barHeight = 20;
    int barX = 20;
    int barY = sh - 40;

    float healthPercent = (localPlayer_.maxHp > 0)
        ? static_cast<float>(localPlayer_.hp) / static_cast<float>(localPlayer_.maxHp)
        : 0.0f;

    batch.drawRect(static_cast<float>(barX), static_cast<float>(barY),
                   static_cast<float>(barWidth), static_cast<float>(barHeight),
                   rf::Color{80, 80, 80, 255});
    batch.drawRect(static_cast<float>(barX), static_cast<float>(barY),
                   static_cast<float>(static_cast<int>(barWidth * healthPercent)),
                   static_cast<float>(barHeight), rf::Color::Red());
    batch.drawRectLines(static_cast<float>(barX), static_cast<float>(barY),
                        static_cast<float>(barWidth), static_cast<float>(barHeight),
                        rf::Color::White());

    // Health text
    char healthText[32];
    std::snprintf(healthText, sizeof(healthText), "%d / %d HP", localPlayer_.hp, localPlayer_.maxHp);
    batch.drawText(healthText, static_cast<float>(barX + barWidth + 10),
                   static_cast<float>(barY + 2), 16, rf::Color::White());

    // Team indicator
    rf::Color teamColor = get_team_color(localPlayer_.team);
    batch.drawRect(static_cast<float>(sw - 120), 20.0f, 100.0f, 30.0f, teamColor);

    const char* teamName = "None";
    switch (localPlayer_.team) {
        case proto::Teams::Red: teamName = "RED"; break;
        case proto::Teams::Blue: teamName = "BLUE"; break;
        case proto::Teams::Green: teamName = "GREEN"; break;
        case proto::Teams::Yellow: teamName = "YELLOW"; break;
        default: break;
    }
    batch.drawText(teamName, static_cast<float>(sw - 110), 25.0f, 20, rf::Color::White());
}

void BedWarsClient::render_debug_info() {
    auto& batch = rf::Batch2D::instance();
    char buf[256];
    float y = 10.0f;

    std::snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
    batch.drawText(buf, 10, y, 16, rf::Color::Green());
    y += 20;

    // Get position and camera from ECS
    if (playerEntity_ != entt::null) {
        if (registry_.all_of<ecs::Transform>(playerEntity_)) {
            auto& pos = registry_.get<ecs::Transform>(playerEntity_).position;
            std::snprintf(buf, sizeof(buf), "Pos: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            batch.drawText(buf, 10, y, 16, rf::Color::White());
            y += 20;
        }
        if (registry_.all_of<ecs::FirstPersonCamera>(playerEntity_)) {
            auto& cam = registry_.get<ecs::FirstPersonCamera>(playerEntity_);
            std::snprintf(buf, sizeof(buf), "Yaw: %.1f  Pitch: %.1f", cam.yaw, cam.pitch);
            batch.drawText(buf, 10, y, 16, rf::Color::White());
            y += 20;
        }
    }

    std::snprintf(buf, sizeof(buf), "Seed: %u", worldSeed_);
    batch.drawText(buf, 10, y, 16, rf::Color::White());
    y += 20;

    std::snprintf(buf, sizeof(buf), "Player ID: %u", localPlayerId_);
    batch.drawText(buf, 10, y, 16, rf::Color::White());
    y += 20;

    // Network info
    std::snprintf(buf, sizeof(buf), "Ping: %u ms", engine_->ping_ms());
    batch.drawText(buf, 10, y, 16, rf::Color::Yellow());
    y += 20;

    std::snprintf(buf, sizeof(buf), "Server Tick: %u", serverTick_);
    batch.drawText(buf, 10, y, 16, rf::Color::White());
    y += 20;

    std::snprintf(buf, sizeof(buf), "Tick Rate: %u Hz", tickRate_);
    batch.drawText(buf, 10, y, 16, rf::Color::White());
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
    rf::Input::instance().setCursorMode(rf::CursorMode::Disabled);
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
            engine_->log(engine::LogLevel::Info, "Quit requested - shutting down");
            engine_->request_shutdown();
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
                rf::Input::instance().setCursorMode(rf::CursorMode::Disabled);
            }
        }
        else if (std::get_if<ui::OpenPauseMenu>(&cmd)) {
            if (sessionState_ == SessionState::InGame) {
                gameScreen_ = ui::GameScreen::Paused;
                rf::Input::instance().setCursorMode(rf::CursorMode::Normal);
            }
        }
        else if (std::get_if<ui::ReturnToMainMenu>(&cmd)) {
            if (onDisconnect_) {
                onDisconnect_();
            }
            gameScreen_ = ui::GameScreen::MainMenu;
            sessionState_ = SessionState::Disconnected;
            rf::Input::instance().setCursorMode(rf::CursorMode::Normal);
        }
    }
}

// ============================================================================
// Helpers
// ============================================================================

rf::Color BedWarsClient::get_team_color(proto::TeamId team) const {
    switch (team) {
        case proto::Teams::Red: return rf::Color::Red();
        case proto::Teams::Blue: return rf::Color::Blue();
        case proto::Teams::Green: return rf::Color::Green();
        case proto::Teams::Yellow: return rf::Color::Yellow();
        default: return rf::Color::White();
    }
}

// TODO: This is a very basic lighting system for demonstration. It can/will be expanded with more lights, colors, and dynamic effects.
void BedWarsClient::update_lights() {
    std::vector<voxel::PointLight> active_lights;
    
    active_lights.reserve(2 + players_.size());

    if (lightingConfig_.enable_player_light && 
        playerEntity_ != entt::null && 
        registry_.all_of<ecs::Transform>(playerEntity_)) {
        auto& transform = registry_.get<ecs::Transform>(playerEntity_);
        active_lights.push_back({
            .position = {transform.position.x, transform.position.y + 1.5f, transform.position.z},
            .color = {1.0f, 0.9f, 0.7f},
            .radius = lightingConfig_.player_light_radius,
            .intensity = lightingConfig_.player_light_intensity
        });
    }

    active_lights.push_back({
        .position = {0.0f, 85.0f, 0.0f},
        .color = {0.0f, 1.0f, 0.0f},
        .radius = 15.0f,
        .intensity = 1.5f
    });

    if (lightingConfig_.enable_other_players_light) {
        for (const auto& [id, state] : players_) {
            if (!state.alive) continue;
            
            active_lights.push_back({
                .position = {state.px, state.py + 1.5f, state.pz},
                .color = {0.5f, 0.7f, 1.0f},
                .radius = 8.0f,
                .intensity = 0.7f
            });
        }
    }

    engine_->world().set_lights(active_lights);
}

} // namespace bedwars
