#include "game.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "../ecs/components.hpp"
#include "../ecs/systems/input_system.hpp"
#include "../ecs/systems/physics_system.hpp"
#include "../ecs/systems/player_system.hpp"
#include "../ecs/systems/render_system.hpp"
#include "../voxel/world.hpp"
#include "../voxel/block_registry.hpp"
#include "../voxel/block_interaction.hpp"
#include <cstdio>
#include <ctime>

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

    // Load client config from rayflow.conf (optional; defaults apply if missing)
    core::Config::instance().load_from_file("rayflow.conf");
    core::Logger::instance().init(core::Config::instance().logging());

    if (session_) {
        session_->start_handshake();
    }
    
    // Initialize block registry
    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        TraceLog(LOG_ERROR, "Failed to initialize block registry!");
        return false;
    }
    
    // Create world
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    world_ = std::make_unique<voxel::World>(seed);
    
    // Create block interaction
    block_interaction_ = std::make_unique<voxel::BlockInteraction>();
    
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

    TraceLog(LOG_INFO, "Game initialized with ECS architecture!");
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
    
    return true;
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
    block_interaction_.reset();
    world_.reset();
    voxel::BlockRegistry::instance().destroy();
    
    input_system_.reset();
    physics_system_.reset();
    player_system_.reset();
    render_system_.reset();

    core::Logger::instance().shutdown();
    
    CloseWindow();
}

void Game::handle_global_input() {
    if (IsKeyPressed(core::Config::instance().controls().exit)) {
        should_exit_ = true;
    }
}

void Game::update(float delta_time) {
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
                physics_system_->set_world(world_.get());
                player_system_->set_world(world_.get());
                render_system_->set_world(world_.get());
            }
        }
    }

    // Update ECS systems
    input_system_->update(registry_, delta_time);
    player_system_->update(registry_, delta_time);
    // physics_system_ is intentionally not run in client replica mode
    
    // Get player position and camera for world updates
    auto& transform = registry_.get<ecs::Transform>(player_entity_);
    auto& fps_camera = registry_.get<ecs::FirstPersonCamera>(player_entity_);
    auto& input = registry_.get<ecs::InputState>(player_entity_);
    auto& tool = registry_.get<ecs::ToolHolder>(player_entity_);

    if (session_) {
        // Note: this is only the plumbing stage; server-authoritative movement comes later.
        session_->send_input(
            input.move_input.x,
            input.move_input.y,
            fps_camera.yaw,
            fps_camera.pitch,
            input.jump_pressed,
            input.sprint_pressed);
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

            // Ensure client-side physics doesn't accumulate stale velocity.
            if (registry_.all_of<ecs::Velocity>(player_entity_)) {
                auto& vel = registry_.get<ecs::Velocity>(player_entity_);
                vel.linear = {0.0f, 0.0f, 0.0f};
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
    
    // Update block interaction
    block_interaction_->update(*world_, camera.position, camera_dir, 
                                tool, input.primary_action, delta_time);
    
    // Update world (chunk loading/unloading)
    world_->update(transform.position);
}

void Game::render() {
    BeginDrawing();
    ClearBackground(Color{135, 206, 235, 255});  // Sky blue
    
    Camera3D camera = ecs::PlayerSystem::get_camera(registry_, player_entity_);
    
    BeginMode3D(camera);
    
    // Render world
    render_system_->render(registry_, camera);
    
    // Render block highlight and break overlay
    block_interaction_->render_highlight(camera);
    block_interaction_->render_break_overlay(camera);
    
    EndMode3D();
    
    // Render UI
    render_system_->render_ui(registry_, screen_width_, screen_height_);
    voxel::BlockInteraction::render_crosshair(screen_width_, screen_height_);
    
    EndDrawing();
}
