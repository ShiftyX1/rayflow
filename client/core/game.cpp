#include "game.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "resources.hpp"
#include "../ecs/components.hpp"
#include "../ecs/systems/input_system.hpp"
#include "../ecs/systems/physics_system.hpp"
#include "../ecs/systems/player_system.hpp"
#include "../ecs/systems/render_system.hpp"
#include "../voxel/world.hpp"
#include "../voxel/block_registry.hpp"
#include "../voxel/block_interaction.hpp"
#include "../renderer/skybox.hpp"
#include "../../shared/maps/runtime_paths.hpp"
#include "../../shared/transport/enet_client.hpp"
#include <cstdio>
#include <ctime>
#include <utility>

Game::Game() = default;
Game::~Game() = default;

void Game::set_transport_endpoint(std::shared_ptr<shared::transport::IEndpoint> endpoint) {
    session_ = std::make_unique<client::net::ClientSession>(std::move(endpoint));
}

bool Game::init(int width, int height, const char* title) {
    screen_width_ = width;
    screen_height_ = height;
    
    InitWindow(width, height, title);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    // Initialize resource system (VFS) - must be after InitWindow.
    resources::init();

    // Load client config from rayflow.conf (optional; defaults apply if missing)
    const bool cfg_ok = core::Config::instance().load_from_file("rayflow.conf");
    core::Logger::instance().init(core::Config::instance().logging());

    TraceLog(LOG_INFO, "[config] %s, render.voxel_smooth_lighting=%s",
             cfg_ok ? "ok" : "missing (defaults)",
             core::Config::instance().get().render.voxel_smooth_lighting ? "true" : "false");

    ui_.init();
    renderer::Skybox::instance().init();

    // Initialize block registry early (needed even before gameplay for potential previews)
    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        TraceLog(LOG_ERROR, "Failed to initialize block registry!");
        return false;
    }

    game_screen_ = ui::GameScreen::MainMenu;
    EnableCursor();
    cursor_enabled_ = true;

    TraceLog(LOG_INFO, "Game initialized! Starting in main menu.");
    
    return true;
}

void Game::start_gameplay() {
    if (gameplay_initialized_) {
        game_screen_ = ui::GameScreen::Playing;
        DisableCursor();
        cursor_enabled_ = false;
        return;
    }

    if (session_) {
        session_->start_handshake();
    }
    
    // Create world
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    world_ = std::make_unique<voxel::World>(seed);
    if (session_) {
        session_->set_on_block_placed([this](const shared::proto::BlockPlaced& ev) {
            if (!world_) return;
            world_->set_block(ev.x, ev.y, ev.z, static_cast<voxel::Block>(ev.blockType));
        });
        session_->set_on_block_broken([this](const shared::proto::BlockBroken& ev) {
            if (!world_) return;
            world_->set_block(ev.x, ev.y, ev.z, static_cast<voxel::Block>(voxel::BlockType::Air));
        });
        session_->set_on_action_rejected([this](const shared::proto::ActionRejected&) {
            if (block_interaction_) block_interaction_->on_action_rejected();
        });
    }
    
    // Create block interaction
    block_interaction_ = std::make_unique<voxel::BlockInteraction>();
    block_interaction_->init();
    
    // Initialize systems
    input_system_ = std::make_unique<ecs::InputSystem>();
    physics_system_ = std::make_unique<ecs::PhysicsSystem>();
    player_system_ = std::make_unique<ecs::PlayerSystem>();
    render_system_ = std::make_unique<ecs::RenderSystem>();

    // Server-authoritative movement: client systems must not simulate movement/physics.
    player_system_->set_client_replica_mode(true);
    
    // Set world reference for systems
    physics_system_->set_world(world_.get());
    player_system_->set_world(world_.get());
    render_system_->set_world(world_.get());
    
    // Create player entity
    Vector3 spawn_position = {50.0f, 80.0f, 50.0f};
    player_entity_ = ecs::PlayerSystem::create_player(registry_, spawn_position);
    
    DisableCursor();
    cursor_enabled_ = false;
    game_screen_ = ui::GameScreen::Playing;
    gameplay_initialized_ = true;

    TraceLog(LOG_INFO, "Gameplay started!");
    TraceLog(LOG_INFO, "Player spawned at (%.1f, %.1f, %.1f)",
             spawn_position.x, spawn_position.y, spawn_position.z);

    const auto& controls = core::Config::instance().controls();
    TraceLog(LOG_INFO, "Controls:");
    TraceLog(LOG_INFO, "  %s/%s/%s/%s - Move player",
             core::key_name(controls.move_forward).c_str(),
             core::key_name(controls.move_left).c_str(),
             core::key_name(controls.move_backward).c_str(),
             core::key_name(controls.move_right).c_str());
    TraceLog(LOG_INFO, "  Mouse - Look around");
    TraceLog(LOG_INFO, "  %s - Jump (or fly up in creative mode)", core::key_name(controls.jump).c_str());
    TraceLog(LOG_INFO, "  %s - Fly down in creative mode", core::key_name(controls.fly_down).c_str());
    TraceLog(LOG_INFO, "  %s - Sprint", core::key_name(controls.sprint).c_str());
    TraceLog(LOG_INFO, "  %s - Toggle creative mode", core::key_name(controls.toggle_creative).c_str());
    TraceLog(LOG_INFO, "  %s - Break block", core::mouse_button_name(controls.primary_mouse).c_str());
    TraceLog(LOG_INFO, "  %s-%s - Select tool",
             core::key_name(controls.tool_1).c_str(),
             core::key_name(controls.tool_5).c_str());
    TraceLog(LOG_INFO, "  %s - Exit", core::key_name(controls.exit).c_str());
}

void Game::set_cursor_enabled(bool enabled) {
    if (enabled == cursor_enabled_) {
        return;
    }

    cursor_enabled_ = enabled;
    if (cursor_enabled_) {
        EnableCursor();
    } else {
        DisableCursor();
        SetMousePosition(screen_width_ / 2, screen_height_ / 2);
    }
}

void Game::apply_ui_commands(const ui::UIFrameOutput& out) {
    for (const auto& cmd : out.commands) {
        if (const auto* s = std::get_if<ui::SetCameraSensitivity>(&cmd)) {
            if (player_entity_ != entt::null && registry_.all_of<ecs::PlayerController>(player_entity_)) {
                registry_.get<ecs::PlayerController>(player_entity_).camera_sensitivity = s->value;
            }
        } else if (std::get_if<ui::StartGame>(&cmd)) {
            start_gameplay();
        } else if (std::get_if<ui::QuitGame>(&cmd)) {
            should_exit_ = true;
        } else if (std::get_if<ui::OpenSettings>(&cmd)) {
            // TODO: Implement settings screen
            TraceLog(LOG_INFO, "Settings not implemented yet");
        } else if (std::get_if<ui::ResumeGame>(&cmd)) {
            if (gameplay_initialized_) {
                game_screen_ = ui::GameScreen::Playing;
                DisableCursor();
                cursor_enabled_ = false;
            }
        }
    }
}

void Game::clear_player_input() {
    if (player_entity_ == entt::null || !registry_.all_of<ecs::InputState>(player_entity_)) {
        return;
    }
    auto& input = registry_.get<ecs::InputState>(player_entity_);
    input.move_input = {0.0f, 0.0f};
    input.look_input = {0.0f, 0.0f};
    input.jump_pressed = false;
    input.sprint_pressed = false;
    input.primary_action = false;
    input.secondary_action = false;
}

void Game::refresh_ui_view_model(float delta_time) {
    ui_vm_.screen_width = screen_width_;
    ui_vm_.screen_height = screen_height_;
    ui_vm_.dt = delta_time;
    ui_vm_.fps = GetFPS();
    ui_vm_.game_screen = game_screen_;

    // Temporary HUD stats (server-authoritative health not implemented yet).
    ui_vm_.player.health = 20;
    ui_vm_.player.max_health = 20;

    if (gameplay_initialized_ && player_entity_ != entt::null) {
        if (registry_.all_of<ecs::Transform>(player_entity_)) {
            ui_vm_.player.position = registry_.get<ecs::Transform>(player_entity_).position;
        }
        if (registry_.all_of<ecs::Velocity>(player_entity_)) {
            ui_vm_.player.velocity = registry_.get<ecs::Velocity>(player_entity_).linear;
        }
        if (registry_.all_of<ecs::PlayerController>(player_entity_)) {
            const auto& pc = registry_.get<ecs::PlayerController>(player_entity_);
            ui_vm_.player.on_ground = pc.on_ground;
            ui_vm_.player.sprinting = pc.is_sprinting;
            ui_vm_.player.creative = pc.in_creative_mode;
            ui_vm_.player.camera_sensitivity = pc.camera_sensitivity;
        }
        if (registry_.all_of<ecs::FirstPersonCamera>(player_entity_)) {
            const auto& cam = registry_.get<ecs::FirstPersonCamera>(player_entity_);
            ui_vm_.player.yaw = cam.yaw;
            ui_vm_.player.pitch = cam.pitch;
        }
    }

    ui_vm_.net = {};
    if (session_) {
        const auto& hello = session_->server_hello();
        ui_vm_.net.has_server_hello = hello.has_value();
        if (hello.has_value()) {
            ui_vm_.net.tick_rate = hello->tickRate;
            ui_vm_.net.world_seed = hello->worldSeed;
        }

        const auto& ack = session_->join_ack();
        ui_vm_.net.has_join_ack = ack.has_value();
        if (ack.has_value()) {
            ui_vm_.net.player_id = ack->playerId;
        }

        const auto& snap = session_->latest_snapshot();
        ui_vm_.net.has_snapshot = snap.has_value();
        if (snap.has_value()) {
            ui_vm_.net.server_tick = snap->serverTick;
        }
    }
}

void Game::run() {
    while (!WindowShouldClose() && !should_exit_) {
        float delta_time = GetFrameTime();
        
        handle_global_input();
        update(delta_time);
        render();
    }
}

void Game::shutdown() {
    if (block_interaction_) {
        block_interaction_->destroy();
    }
    block_interaction_.reset();
    world_.reset();
    voxel::BlockRegistry::instance().destroy();
    
    input_system_.reset();
    physics_system_.reset();
    player_system_.reset();
    render_system_.reset();
    renderer::Skybox::instance().shutdown();

    core::Logger::instance().shutdown();

    // Shutdown resource system (VFS) before CloseWindow.
    resources::shutdown();
    
    CloseWindow();
}

void Game::handle_global_input() {
    if (IsKeyPressed(core::Config::instance().controls().exit)) {
        should_exit_ = true;
    }
}

void Game::update(float delta_time) {
    // Ensure UI has valid dimensions even before we build a full view-model.
    ui_vm_.screen_width = screen_width_;
    ui_vm_.screen_height = screen_height_;
    ui_vm_.dt = delta_time;
    ui_vm_.fps = GetFPS();
    ui_vm_.game_screen = game_screen_;

    // UI: toggle + capture + apply commands from last frame (safe point).
    ui::UIFrameInput ui_in;
    ui_in.dt = delta_time;
    ui_in.toggle_debug_ui = IsKeyPressed(KEY_F1);
    ui_in.toggle_debug_overlay = IsKeyPressed(KEY_F2);

    const ui::UIFrameOutput ui_out = ui_.update(ui_in, ui_vm_);
    ui_captures_input_ = ui_out.capture.captured();
    apply_ui_commands(ui_out);

    set_cursor_enabled(ui_out.capture.wants_mouse);

    if (game_screen_ == ui::GameScreen::MainMenu) {
        refresh_ui_view_model(delta_time);
        return;
    }

    if (!gameplay_initialized_) {
        return;
    }

    // Poll ENet for network events (must be called every frame)
    if (netClient_) {
        netClient_->poll(0);
    }

    if (session_) {
        session_->poll();
    }

    // Ensure the client render-world matches the authoritative server seed.
    if (session_) {
        const auto& helloOpt = session_->server_hello();
        if (helloOpt.has_value()) {
            const unsigned int desiredSeed = static_cast<unsigned int>(helloOpt->worldSeed);
            if (!world_ || world_->get_seed() != desiredSeed) {
                world_ = std::make_unique<voxel::World>(desiredSeed);
                if (physics_system_) physics_system_->set_world(world_.get());
                if (player_system_) player_system_->set_world(world_.get());
                if (render_system_) render_system_->set_world(world_.get());
            }

            // MT-1: if server advertises a finite map template, load it locally for rendering.
            if (helloOpt->hasMapTemplate && world_) {
                const auto* cur = world_->map_template();
                const bool mismatch = (!cur) || cur->mapId != helloOpt->mapId || cur->version != helloOpt->mapVersion;
                if (mismatch) {
                    const std::string fileName = helloOpt->mapId + "_v" + std::to_string(helloOpt->mapVersion) + ".rfmap";
                    const std::filesystem::path path = shared::maps::runtime_maps_dir() / fileName;

                    shared::maps::MapTemplate map;
                    std::string err;
                    if (shared::maps::read_rfmap(path, &map, &err)) {
                        world_->set_map_template(std::move(map));

                        if (const auto* mt = world_->map_template()) {
                            const auto& vs = mt->visualSettings;
                            renderer::Skybox::instance().set_kind(vs.skyboxKind);
                        }

                        TraceLog(LOG_INFO, "[map] loaded template: %s", path.generic_string().c_str());
                    } else {
                        TraceLog(LOG_WARNING, "[map] failed to load template %s: %s", path.generic_string().c_str(), err.c_str());
                    }
                }
            } else if (world_) {
                // No template advertised: ensure we render the procedural world.
                if (world_->has_map_template()) {
                    world_->clear_map_template();
                }

                // Reset MV-2 tint parameter back to neutral for procedural worlds.
            }
        }
    }

    // Update ECS systems
    if (!ui_captures_input_ && input_system_ && player_system_) {
        input_system_->update(registry_, delta_time);
        player_system_->update(registry_, delta_time);
    } else {
        clear_player_input();
    }
    // physics_system_ is intentionally not run in client replica mode
    
    // Get player position and camera for world updates
    if (player_entity_ == entt::null) {
        refresh_ui_view_model(delta_time);
        return;
    }

    auto& transform = registry_.get<ecs::Transform>(player_entity_);
    auto& fps_camera = registry_.get<ecs::FirstPersonCamera>(player_entity_);
    auto& input = registry_.get<ecs::InputState>(player_entity_);
    auto& tool = registry_.get<ecs::ToolHolder>(player_entity_);

    if (session_) {
        // Note: server-authoritative movement; client sends intent only.
        session_->send_input(
            ui_captures_input_ ? 0.0f : input.move_input.x,
            ui_captures_input_ ? 0.0f : input.move_input.y,
            fps_camera.yaw,
            fps_camera.pitch,
            ui_captures_input_ ? false : input.jump_pressed,
            ui_captures_input_ ? false : input.sprint_pressed);
    }

    // Apply authoritative position from the server snapshot.
    if (session_) {
        const auto& snapOpt = session_->latest_snapshot();
        if (snapOpt.has_value()) {
            const auto& snap = *snapOpt;
            const float targetX = snap.px;
            const float targetY = snap.py;
            const float targetZ = snap.pz;

            // Simple interpolation (no prediction): critically damped-ish lerp.
            const float t = (delta_time <= 0.0f) ? 1.0f : (delta_time * 15.0f);
            const float alpha = (t > 1.0f) ? 1.0f : t;

            transform.position.x = transform.position.x + (targetX - transform.position.x) * alpha;
            transform.position.y = transform.position.y + (targetY - transform.position.y) * alpha;
            transform.position.z = transform.position.z + (targetZ - transform.position.z) * alpha;

            // Replicate authoritative velocity for UI/debug display.
            if (registry_.all_of<ecs::Velocity>(player_entity_)) {
                auto& vel = registry_.get<ecs::Velocity>(player_entity_);
                vel.linear = {snap.vx, snap.vy, snap.vz};
            }
        }
    }
    
    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, player_entity_);
    
    // Calculate camera direction
    Vector3 camera_dir = {
        camera.target.x - camera.position.x,
        camera.target.y - camera.position.y,
        camera.target.z - camera.position.z
    };
    
    if (!ui_captures_input_ && block_interaction_ && world_) {
        // Update block interaction
        block_interaction_->update(*world_, camera.position, camera_dir,
                                    tool, input.primary_action, input.secondary_action, delta_time);

        if (session_) {
            if (auto req = block_interaction_->consume_break_request(); req.has_value()) {
                session_->send_try_break_block(req->x, req->y, req->z);
            }
            if (auto req = block_interaction_->consume_place_request(); req.has_value()) {
                session_->send_try_place_block(req->x, req->y, req->z, req->block_type);
            }
        }
    }
    
    // Update world (chunk loading/unloading)
    if (world_) {
        world_->update(transform.position);
    }

    // Build a fresh view-model for render() (debug overlays, stats, etc.)
    refresh_ui_view_model(delta_time);
}

void Game::render() {
    BeginDrawing();
    ClearBackground(BLACK);
    
    // If in main menu, just render UI
    if (game_screen_ == ui::GameScreen::MainMenu) {
        ui_.render(ui_vm_);
        EndDrawing();
        return;
    }

    // From here on: gameplay rendering
    if (!gameplay_initialized_ || player_entity_ == entt::null) {
        ui_.render(ui_vm_);
        EndDrawing();
        return;
    }

    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, player_entity_);
    
    BeginMode3D(camera);

    renderer::Skybox::instance().draw(camera);
    
    // Render world
    if (render_system_) {
        render_system_->render(registry_, camera);
    }
    
    // Render block highlight and break overlay
    if (block_interaction_) {
        block_interaction_->render_highlight(camera);
        block_interaction_->render_break_overlay(camera);
    }
    
    EndMode3D();
    
    // Render UI
    if (render_system_) {
        render_system_->render_ui(registry_, screen_width_, screen_height_);
    }

    ui_.render(ui_vm_);
    
    EndDrawing();
}
