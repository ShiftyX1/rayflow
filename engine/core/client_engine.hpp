#pragma once

#include "game_interface.hpp"
#include "../transport/transport.hpp"

#include <entt/entt.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>

// Forward declarations
namespace voxel {
    class World;
    class BlockInteraction;
    class BlockRegistry;
    class BlockModelLoader;
}

namespace ui {
    class UIManager;
}

namespace renderer {
    class Skybox;
}

namespace engine {

// ============================================================================
// ClientEngine - Full-featured voxel game client engine
// ============================================================================

class ClientEngine : public IClientServices {
public:
    struct Config {
        int windowWidth = 1280;
        int windowHeight = 720;
        std::string windowTitle = "Game";
        int targetFps = 60;
        bool vsync = true;
        bool logging = true;
        std::string configFile = "rayflow.conf";
    };

    ClientEngine();
    explicit ClientEngine(const Config& config);
    ~ClientEngine();

    /// Set the transport (must be called before run if networking is needed).
    void set_transport(std::shared_ptr<transport::IClientTransport> transport);

    /// Run the client with the given game.
    /// This runs the render loop on the current thread (blocking).
    void run(IGameClient& game);

    /// Request shutdown (can be called from game or another thread).
    void stop();

    // --- IClientServices implementation ---
    
    void send(std::span<const std::uint8_t> data) override;
    ConnectionState connection_state() const override;
    std::uint32_t ping_ms() const override;
    float frame_dt() const override { return frameDt_; }
    
    int window_width() const override { return config_.windowWidth; }
    int window_height() const override { return config_.windowHeight; }
    
    voxel::World& world() override;
    const voxel::World& world() const override;
    void init_world(std::uint32_t seed) override;
    voxel::BlockInteraction& block_interaction() override;
    
    entt::registry& registry() override;
    
    ui::UIManager& ui_manager() override;
    
    void log(LogLevel level, std::string_view msg) override;

private:
    void init_window();
    void init_subsystems();
    void close_window();
    void shutdown_subsystems();
    void main_loop(IGameClient& game);

    Config config_;
    
    // Transport
    std::shared_ptr<transport::IClientTransport> transport_;
    IGameClient* game_{nullptr};
    
    // Engine state
    std::atomic<bool> running_{false};
    float frameDt_{0.0f};
    ConnectionState connectionState_{ConnectionState::Disconnected};
    
    // ECS
    entt::registry registry_;
    
    // Voxel subsystems (owned by engine)
    std::unique_ptr<voxel::World> world_;
    std::unique_ptr<voxel::BlockInteraction> blockInteraction_;
    
    // UI subsystem (owned by engine)
    std::unique_ptr<ui::UIManager> uiManager_;
};

} // namespace engine
