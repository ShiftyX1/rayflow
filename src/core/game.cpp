#include "game.hpp"
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

bool Game::init(int width, int height, const char* title) {
    screen_width_ = width;
    screen_height_ = height;
    
    InitWindow(width, height, title);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    
    // Initialize block registry
    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        std::fprintf(stderr, "Failed to initialize block registry!\n");
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
    
    // Set world reference for systems
    physics_system_->set_world(world_.get());
    player_system_->set_world(world_.get());
    render_system_->set_world(world_.get());
    
    // Create player entity
    Vector3 spawn_position = {50.0f, 80.0f, 50.0f};
    player_entity_ = ecs::PlayerSystem::create_player(registry_, spawn_position);
    
    DisableCursor();
    
    std::printf("Game initialized with ECS architecture!\n");
    std::printf("Player spawned at (%.1f, %.1f, %.1f)\n", 
                spawn_position.x, spawn_position.y, spawn_position.z);
    std::printf("\nControls:\n");
    std::printf("  WASD - Move player\n");
    std::printf("  Mouse - Look around\n");
    std::printf("  Space - Jump (or fly up in creative mode)\n");
    std::printf("  Left Shift - Fly down in creative mode\n");
    std::printf("  Left Ctrl - Sprint\n");
    std::printf("  C - Toggle creative mode\n");
    std::printf("  Left Mouse Button - Break block\n");
    std::printf("  1-5 - Select tool\n");
    std::printf("  ESC - Exit\n");
    
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
    
    CloseWindow();
}

void Game::handle_global_input() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        should_exit_ = true;
    }
}

void Game::update(float delta_time) {
    // Update ECS systems
    input_system_->update(registry_, delta_time);
    player_system_->update(registry_, delta_time);
    physics_system_->update(registry_, delta_time);
    
    // Get player position and camera for world updates
    auto& transform = registry_.get<ecs::Transform>(player_entity_);
    auto& fps_camera = registry_.get<ecs::FirstPersonCamera>(player_entity_);
    auto& input = registry_.get<ecs::InputState>(player_entity_);
    auto& tool = registry_.get<ecs::ToolHolder>(player_entity_);
    
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
