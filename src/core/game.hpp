#pragma once

#include <entt/entt.hpp>
#include <raylib.h>
#include <memory>

// Forward declarations
namespace voxel {
    class World;
    class BlockInteraction;
}

namespace ecs {
    class InputSystem;
    class PhysicsSystem;
    class PlayerSystem;
    class RenderSystem;
}

class Game {
public:
    Game();
    ~Game();
    
    bool init(int width, int height, const char* title);
    void run();
    void shutdown();
    
private:
    void update(float delta_time);
    void render();
    void handle_global_input();
    
    // Window
    int screen_width_{1280};
    int screen_height_{720};
    bool should_exit_{false};
    
    // ECS
    entt::registry registry_;
    entt::entity player_entity_{entt::null};
    
    // Systems
    std::unique_ptr<ecs::InputSystem> input_system_;
    std::unique_ptr<ecs::PhysicsSystem> physics_system_;
    std::unique_ptr<ecs::PlayerSystem> player_system_;
    std::unique_ptr<ecs::RenderSystem> render_system_;
    
    // Voxel world
    std::unique_ptr<voxel::World> world_;
    std::unique_ptr<voxel::BlockInteraction> block_interaction_;
};
