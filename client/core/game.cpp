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
#include "../voxel/block_model_loader.hpp"
#include "../voxel/block_interaction.hpp"
#include "../renderer/skybox.hpp"
#include "../../shared/maps/runtime_paths.hpp"
#include "../../shared/transport/enet_client.hpp"
#include "../../shared/transport/enet_common.hpp"
#include "../../shared/transport/local_transport.hpp"
#include "../../shared/voxel/block_state.hpp"
#include "../../server/core/server.hpp"
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

    resources::init();

    const bool cfg_ok = core::Config::instance().load_from_file("rayflow.conf");
    core::Logger::instance().init(core::Config::instance().logging());

    TraceLog(LOG_INFO, "[config] %s, render.voxel_smooth_lighting=%s",
             cfg_ok ? "ok" : "missing (defaults)",
             core::Config::instance().get().render.voxel_smooth_lighting ? "true" : "false");

    ui_.init();
    renderer::Skybox::instance().init();

    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        TraceLog(LOG_ERROR, "Failed to initialize block registry!");
        return false;
    }
    
    if (!voxel::BlockModelLoader::instance().init()) {
        TraceLog(LOG_WARNING, "Failed to initialize block model loader (non-full blocks may render incorrectly)");
    }

    game_screen_ = ui::GameScreen::MainMenu;
    EnableCursor();
    cursor_enabled_ = true;

    TraceLog(LOG_INFO, "Game initialized! Starting in main menu.");
    
    return true;
}

void Game::start_singleplayer() {
    TraceLog(LOG_INFO, "Starting singleplayer...");
    
    auto pair = shared::transport::LocalTransport::create_pair();

    server::core::Server::Options sv_opts;
    {
        const auto& sv = core::Config::instance().sv_logging();
        sv_opts.logging.enabled = sv.enabled;
        sv_opts.logging.init = sv.init;
        sv_opts.logging.rx = sv.rx;
        sv_opts.logging.tx = sv.tx;
        sv_opts.logging.move = sv.move;
        sv_opts.logging.coll = sv.coll;
    }

    local_server_ = std::make_unique<server::core::Server>(pair.server, sv_opts);
    local_server_->start();

    session_ = std::make_unique<client::net::ClientSession>(pair.client);
    netClient_ = nullptr;
    is_multiplayer_ = false;

    TraceLog(LOG_INFO, "Singleplayer server started.");
    start_gameplay();
}

void Game::connect_to_server(const std::string& host, std::uint16_t port) {
    TraceLog(LOG_INFO, "Connecting to %s:%u...", host.c_str(), port);
    
    game_screen_ = ui::GameScreen::Connecting;
    connection_error_.clear();
    
    if (!enet_init_) {
        enet_init_ = std::make_unique<shared::transport::ENetInitializer>();
        if (!enet_init_->isInitialized()) {
            connection_error_ = "Failed to initialize network";
            TraceLog(LOG_ERROR, "[net] %s", connection_error_.c_str());
            return;
        }
    }
    
    owned_net_client_ = std::make_unique<shared::transport::ENetClient>();
    if (!owned_net_client_->connect(host, port, 5000)) {
        connection_error_ = "Failed to connect to " + host + ":" + std::to_string(port);
        TraceLog(LOG_ERROR, "[net] %s", connection_error_.c_str());
        owned_net_client_.reset();
        return;
    }
    
    TraceLog(LOG_INFO, "[net] Connected to %s:%u", host.c_str(), port);
    
    session_ = std::make_unique<client::net::ClientSession>(owned_net_client_->connection());
    netClient_ = owned_net_client_.get();
    is_multiplayer_ = true;
    
    start_gameplay();
}

void Game::disconnect_from_server() {
    TraceLog(LOG_INFO, "Disconnecting from server...");
    
    if (owned_net_client_) {
        owned_net_client_->disconnect();
        owned_net_client_.reset();
    }
    netClient_ = nullptr;
    session_.reset();
    
    game_screen_ = ui::GameScreen::MainMenu;
    connection_error_.clear();
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
    
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    world_ = std::make_unique<voxel::World>(seed);
    if (session_) {
        session_->set_on_block_placed([this](const shared::proto::BlockPlaced& ev) {
            if (!world_) return;
            auto state = shared::voxel::BlockRuntimeState::from_byte(ev.stateByte);
            world_->set_block_with_state(ev.x, ev.y, ev.z, static_cast<voxel::Block>(ev.blockType), state);
        });
        session_->set_on_block_broken([this](const shared::proto::BlockBroken& ev) {
            if (!world_) return;
            world_->set_block(ev.x, ev.y, ev.z, static_cast<voxel::Block>(voxel::BlockType::Air));
        });
        session_->set_on_chunk_data([this](const shared::proto::ChunkData& cd) {
            if (!world_) return;
            world_->apply_chunk_data(cd.chunkX, cd.chunkZ, cd.blocks);
        });
        session_->set_on_action_rejected([this](const shared::proto::ActionRejected&) {
            if (block_interaction_) block_interaction_->on_action_rejected();
        });
        
        // Game event callbacks
        session_->set_on_team_assigned([this](const shared::proto::TeamAssigned& ev) {
            // Update local player's team if it's us
            if (session_->join_ack() && ev.playerId == session_->join_ack()->playerId) {
                ui_vm_.player.team_id = ev.teamId;
            }
        });
        session_->set_on_health_update([this](const shared::proto::HealthUpdate& ev) {
            // Update local player's health if it's us
            if (session_->join_ack() && ev.playerId == session_->join_ack()->playerId) {
                ui_vm_.player.health = ev.hp;
                ui_vm_.player.max_health = ev.maxHp;
            }
        });
        session_->set_on_player_died([this](const shared::proto::PlayerDied& ev) {
            // Add to kill feed
            ui::KillFeedEntry entry;
            entry.killer_id = ev.killerId;
            entry.victim_id = ev.victimId;
            entry.is_final_kill = ev.isFinalKill;
            entry.time_remaining = 5.0f;
            ui_vm_.game.kill_feed.insert(ui_vm_.game.kill_feed.begin(), entry);
            
            // Limit kill feed size
            if (ui_vm_.game.kill_feed.size() > 5) {
                ui_vm_.game.kill_feed.pop_back();
            }
        });
        session_->set_on_bed_destroyed([this](const shared::proto::BedDestroyed& ev) {
            // Update bed status
            if (ev.teamId <= shared::game::Teams::MaxTeams) {
                ui_vm_.game.team_beds[ev.teamId] = false;
            }
            
            // If our bed was destroyed, update can_respawn
            if (ev.teamId == ui_vm_.player.team_id) {
                ui_vm_.player.can_respawn = false;
                
                // Add notification
                ui::GameNotification notif;
                notif.message = "Your bed was destroyed!";
                notif.color = RED;
                notif.time_remaining = 5.0f;
                ui_vm_.game.notifications.push_back(notif);
            } else {
                // Another team's bed destroyed
                ui::GameNotification notif;
                notif.message = std::string(shared::game::team_name(ev.teamId)) + " bed destroyed!";
                auto color = shared::game::TeamColor::from_team_id(ev.teamId);
                notif.color = Color{color.r, color.g, color.b, 255};
                notif.time_remaining = 3.0f;
                ui_vm_.game.notifications.push_back(notif);
            }
        });
        session_->set_on_team_eliminated([this](const shared::proto::TeamEliminated& ev) {
            ui::GameNotification notif;
            notif.message = std::string(shared::game::team_name(ev.teamId)) + " has been eliminated!";
            auto color = shared::game::TeamColor::from_team_id(ev.teamId);
            notif.color = Color{color.r, color.g, color.b, 255};
            notif.time_remaining = 4.0f;
            ui_vm_.game.notifications.push_back(notif);
        });
        session_->set_on_match_ended([this](const shared::proto::MatchEnded& ev) {
            ui_vm_.game.match_ended = true;
            ui_vm_.game.winner_team = ev.winnerTeamId;
            
            ui::GameNotification notif;
            if (ev.winnerTeamId == ui_vm_.player.team_id) {
                notif.message = "Victory!";
                notif.color = GOLD;
            } else {
                notif.message = std::string(shared::game::team_name(ev.winnerTeamId)) + " wins!";
                auto color = shared::game::TeamColor::from_team_id(ev.winnerTeamId);
                notif.color = Color{color.r, color.g, color.b, 255};
            }
            notif.time_remaining = 10.0f;
            ui_vm_.game.notifications.push_back(notif);
        });
        session_->set_on_inventory_update([this](const shared::proto::InventoryUpdate& ev) {
            // Update resource counts for local player
            if (session_->join_ack() && ev.playerId == session_->join_ack()->playerId) {
                switch (ev.itemType) {
                    case shared::game::ItemType::Iron:
                        ui_vm_.player.iron = ev.count;
                        break;
                    case shared::game::ItemType::Gold:
                        ui_vm_.player.gold = ev.count;
                        break;
                    case shared::game::ItemType::Diamond:
                        ui_vm_.player.diamond = ev.count;
                        break;
                    case shared::game::ItemType::Emerald:
                        ui_vm_.player.emerald = ev.count;
                        break;
                    default:
                        break;
                }
            }
        });
    }
    
    block_interaction_ = std::make_unique<voxel::BlockInteraction>();
    block_interaction_->init();
    
    input_system_ = std::make_unique<ecs::InputSystem>();
    physics_system_ = std::make_unique<ecs::PhysicsSystem>();
    player_system_ = std::make_unique<ecs::PlayerSystem>();
    render_system_ = std::make_unique<ecs::RenderSystem>();

    player_system_->set_client_replica_mode(true);
    
    physics_system_->set_world(world_.get());
    player_system_->set_world(world_.get());
    render_system_->set_world(world_.get());
    
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
    TraceLog(LOG_INFO, "  %s - Pause menu", core::key_name(controls.exit).c_str());
}

void Game::return_to_main_menu() {
    TraceLog(LOG_INFO, "Returning to main menu...");

    if (block_interaction_) {
        block_interaction_->destroy();
        block_interaction_.reset();
    }

    input_system_.reset();
    physics_system_.reset();
    player_system_.reset();
    render_system_.reset();

    registry_.clear();
    player_entity_ = entt::null;

    world_.reset();

    session_.reset();

    if (is_multiplayer_ && owned_net_client_) {
        owned_net_client_->disconnect();
        owned_net_client_.reset();
    }
    netClient_ = nullptr;
    
    if (local_server_) {
        local_server_->stop();
        local_server_.reset();
    }

    is_multiplayer_ = false;
    connection_error_.clear();

    gameplay_initialized_ = false;
    game_screen_ = ui::GameScreen::MainMenu;

    EnableCursor();
    cursor_enabled_ = true;

    TraceLog(LOG_INFO, "Returned to main menu.");
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
            start_singleplayer();
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
        } else if (std::get_if<ui::OpenPauseMenu>(&cmd)) {
            if (gameplay_initialized_) {
                game_screen_ = ui::GameScreen::Paused;
                EnableCursor();
                cursor_enabled_ = true;
            }
        } else if (std::get_if<ui::ReturnToMainMenu>(&cmd)) {
            return_to_main_menu();
        } else if (std::get_if<ui::ShowConnectScreen>(&cmd)) {
            game_screen_ = ui::GameScreen::ConnectMenu;
            connection_error_.clear();
        } else if (std::get_if<ui::HideConnectScreen>(&cmd)) {
            game_screen_ = ui::GameScreen::MainMenu;
            connection_error_.clear();
        } else if (const auto* c = std::get_if<ui::ConnectToServer>(&cmd)) {
            connect_to_server(c->host, c->port);
        } else if (std::get_if<ui::DisconnectFromServer>(&cmd)) {
            disconnect_from_server();
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

    // Note: health, max_health, team_id, etc. are updated by network callbacks
    // Don't overwrite them here

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

    // Update kill feed timers and remove expired entries
    for (auto it = ui_vm_.game.kill_feed.begin(); it != ui_vm_.game.kill_feed.end(); ) {
        it->time_remaining -= delta_time;
        if (it->time_remaining <= 0.0f) {
            it = ui_vm_.game.kill_feed.erase(it);
        } else {
            ++it;
        }
    }
    
    // Update notification timers and remove expired entries
    for (auto it = ui_vm_.game.notifications.begin(); it != ui_vm_.game.notifications.end(); ) {
        it->time_remaining -= delta_time;
        if (it->time_remaining <= 0.0f) {
            it = ui_vm_.game.notifications.erase(it);
        } else {
            ++it;
        }
    }

    ui_vm_.net = {};
    ui_vm_.net.is_connecting = (game_screen_ == ui::GameScreen::Connecting);
    ui_vm_.net.connection_failed = !connection_error_.empty();
    ui_vm_.net.connection_error = connection_error_;
    
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
        if (is_multiplayer_ && owned_net_client_ && owned_net_client_->is_connected()) {
            ui_vm_.net.is_remote_connection = true;
            ui_vm_.net.ping_ms = owned_net_client_->connection()->ping_ms();
        } else {
            ui_vm_.net.is_remote_connection = false;
            ui_vm_.net.ping_ms = 0;
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

    session_.reset();
    
    if (owned_net_client_) {
        owned_net_client_->disconnect();
        owned_net_client_.reset();
    }
    netClient_ = nullptr;
    enet_init_.reset();
    
    if (local_server_) {
        local_server_->stop();
        local_server_.reset();
    }

    core::Logger::instance().shutdown();

    resources::shutdown();
    
    CloseWindow();
}

void Game::handle_global_input() {
    // ESC is now handled by UI (pause menu toggle)
    // Quit only via menu or window close
}

void Game::update(float delta_time) {
    ui_vm_.screen_width = screen_width_;
    ui_vm_.screen_height = screen_height_;
    ui_vm_.dt = delta_time;
    ui_vm_.fps = GetFPS();
    ui_vm_.game_screen = game_screen_;

    ui::UIFrameInput ui_in;
    ui_in.dt = delta_time;
    ui_in.toggle_debug_ui = IsKeyPressed(KEY_F1);
    ui_in.toggle_debug_overlay = IsKeyPressed(KEY_F2);
    ui_in.toggle_pause = IsKeyPressed(core::Config::instance().controls().exit);

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

    if (netClient_) {
        netClient_->poll(0);
    }

    if (session_) {
        session_->poll();
    }

    if (session_) {
        const auto& helloOpt = session_->server_hello();
        if (helloOpt.has_value()) {
            const unsigned int desiredSeed = static_cast<unsigned int>(helloOpt->worldSeed);
            if (!world_ || world_->get_seed() != desiredSeed) {
                world_ = std::make_unique<voxel::World>(desiredSeed);
                if (physics_system_) physics_system_->set_world(world_.get());
                if (player_system_) player_system_->set_world(world_.get());
                if (render_system_) render_system_->set_world(world_.get());

                for (const auto& cd : session_->take_pending_chunk_data()) {
                    world_->apply_chunk_data(cd.chunkX, cd.chunkZ, cd.blocks);
                }

                for (const auto& ev : session_->take_pending_block_placed()) {
                    auto state = shared::voxel::BlockRuntimeState::from_byte(ev.stateByte);
                    world_->set_block_with_state(ev.x, ev.y, ev.z, static_cast<voxel::Block>(ev.blockType), state);
                }
                for (const auto& ev : session_->take_pending_block_broken()) {
                    world_->set_block(ev.x, ev.y, ev.z, static_cast<voxel::Block>(voxel::BlockType::Air));
                }
            }

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
                if (world_->has_map_template()) {
                    world_->clear_map_template();
                }

            }
        }
    }

    if (!ui_captures_input_ && input_system_ && player_system_) {
        input_system_->update(registry_, delta_time);
        player_system_->update(registry_, delta_time);
    } else {
        clear_player_input();
    }
    
    if (player_entity_ == entt::null) {
        refresh_ui_view_model(delta_time);
        return;
    }

    auto& transform = registry_.get<ecs::Transform>(player_entity_);
    auto& fps_camera = registry_.get<ecs::FirstPersonCamera>(player_entity_);
    auto& input = registry_.get<ecs::InputState>(player_entity_);
    auto& tool = registry_.get<ecs::ToolHolder>(player_entity_);

    if (session_) {
        session_->send_input(
            ui_captures_input_ ? 0.0f : input.move_input.x,
            ui_captures_input_ ? 0.0f : input.move_input.y,
            fps_camera.yaw,
            fps_camera.pitch,
            ui_captures_input_ ? false : input.jump_pressed,
            ui_captures_input_ ? false : input.sprint_pressed);
    }

    if (session_) {
        const auto& snapOpt = session_->latest_snapshot();
        if (snapOpt.has_value()) {
            const auto& snap = *snapOpt;
            const float targetX = snap.px;
            const float targetY = snap.py;
            const float targetZ = snap.pz;

            const float t = (delta_time <= 0.0f) ? 1.0f : (delta_time * 15.0f);
            const float alpha = (t > 1.0f) ? 1.0f : t;

            transform.position.x = transform.position.x + (targetX - transform.position.x) * alpha;
            transform.position.y = transform.position.y + (targetY - transform.position.y) * alpha;
            transform.position.z = transform.position.z + (targetZ - transform.position.z) * alpha;

            if (registry_.all_of<ecs::Velocity>(player_entity_)) {
                auto& vel = registry_.get<ecs::Velocity>(player_entity_);
                vel.linear = {snap.vx, snap.vy, snap.vz};
            }
        }
    }
    
    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, player_entity_);
    
    Vector3 camera_dir = {
        camera.target.x - camera.position.x,
        camera.target.y - camera.position.y,
        camera.target.z - camera.position.z
    };
    
    if (!ui_captures_input_ && block_interaction_ && world_) {
        block_interaction_->update(*world_, camera.position, camera_dir,
                                    tool, input.primary_action, input.secondary_action, delta_time);

        if (session_) {
            if (auto req = block_interaction_->consume_break_request(); req.has_value()) {
                session_->send_try_break_block(req->x, req->y, req->z);
            }
            if (auto req = block_interaction_->consume_place_request(); req.has_value()) {
                session_->send_try_place_block(req->x, req->y, req->z, req->block_type, req->hitY, req->face);
            }
        }
    }
    
    if (world_) {
        world_->update(transform.position);
    }

    refresh_ui_view_model(delta_time);
}

void Game::render() {
    BeginDrawing();
    ClearBackground(BLACK);
    
    if (game_screen_ == ui::GameScreen::MainMenu) {
        ui_.render(ui_vm_);
        EndDrawing();
        return;
    }

    if (!gameplay_initialized_ || player_entity_ == entt::null) {
        ui_.render(ui_vm_);
        EndDrawing();
        return;
    }

    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, player_entity_);
    
    BeginMode3D(camera);

    renderer::Skybox::instance().draw(camera);
    
    if (render_system_) {
        render_system_->render(registry_, camera);
    }
    
    if (block_interaction_) {
        block_interaction_->render_highlight(camera);
        block_interaction_->render_break_overlay(camera);
    }
    
    EndMode3D();
    
    if (render_system_) {
        render_system_->render_ui(registry_, screen_width_, screen_height_);
    }

    ui_.render(ui_vm_);
    
    EndDrawing();
}
