#pragma once

#include <entt/entt.hpp>
#include <raylib.h>
#include <memory>
#include <string>

#include "../net/client_session.hpp"

#include "runtime/ui_manager.hpp"

// Forward declarations
namespace shared::transport {
    class ENetClient;
    class ENetInitializer;
}

namespace server::core {
    class Server;
}

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

    void set_transport_endpoint(std::shared_ptr<shared::transport::IEndpoint> endpoint);
    void set_network_client(shared::transport::ENetClient* client) { netClient_ = client; }
    
    bool init(int width, int height, const char* title);
    void run();
    void shutdown();
    
private:
    void update(float delta_time);
    void render();
    void handle_global_input();

    void apply_ui_commands(const ui::UIFrameOutput& out);
    void clear_player_input();
    void refresh_ui_view_model(float delta_time);
    void set_cursor_enabled(bool enabled);

    // Start singleplayer (embedded server)
    void start_singleplayer();
    
    // Start multiplayer connection
    void connect_to_server(const std::string& host, std::uint16_t port);
    void disconnect_from_server();
    
    // Start actual gameplay (called after transport is ready)
    void start_gameplay();
    
    // Return to main menu (cleanup gameplay state)
    void return_to_main_menu();
    
    // Window
    int screen_width_{1280};
    int screen_height_{720};
    bool should_exit_{false};
    
    // Game state
    ui::GameScreen game_screen_{ui::GameScreen::MainMenu};
    bool gameplay_initialized_{false};
    
    // Multiplayer state
    bool is_multiplayer_{false};
    std::string connection_error_{};
    std::unique_ptr<shared::transport::ENetInitializer> enet_init_{};
    std::unique_ptr<shared::transport::ENetClient> owned_net_client_{};
    
    // Singleplayer embedded server
    std::unique_ptr<server::core::Server> local_server_{};
    
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

    std::unique_ptr<client::net::ClientSession> session_;
    shared::transport::ENetClient* netClient_{nullptr};  // Non-owning, for polling

    ui::UIManager ui_{};
    ui::UIViewModel ui_vm_{};
    bool ui_captures_input_{false};
    bool cursor_enabled_{false};
};
