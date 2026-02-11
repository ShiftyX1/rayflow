#include "client_engine.hpp"

#include "engine/client/core/config.hpp"
#include "engine/client/core/logger.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/modules/voxel/client/world.hpp"
#include "engine/modules/voxel/client/block_interaction.hpp"
#include "engine/modules/voxel/client/block_registry.hpp"
#include "engine/modules/voxel/client/block_model_loader.hpp"
#include "engine/renderer/skybox.hpp"
#include "engine/ui/runtime/ui_manager.hpp"

#include "engine/client/core/window.hpp"
#include "engine/client/core/input.hpp"

#include "engine/core/math_types.hpp"
#include "engine/core/key_codes.hpp"
#include "engine/core/logging.hpp"

#include <glad/gl.h>

#include <chrono>
#include <cstdio>

namespace engine {

// ============================================================================
// Lifecycle
// ============================================================================

ClientEngine::ClientEngine()
    : config_()
{
}

ClientEngine::ClientEngine(const Config& config)
    : config_(config)
{
}

ClientEngine::~ClientEngine() {
    stop();
}

void ClientEngine::set_transport(std::shared_ptr<transport::IClientTransport> transport) {
    transport_ = std::move(transport);
    
    if (transport_) {
        // Setup callbacks
        transport_->onConnect = [this]() {
            connectionState_ = ConnectionState::Connected;
            log(LogLevel::Info, "Connected to server");
            if (game_) {
                game_->on_connected();
            }
        };
        
        transport_->onDisconnect = [this]() {
            connectionState_ = ConnectionState::Disconnected;
            log(LogLevel::Info, "Disconnected from server");
            if (game_) {
                game_->on_disconnected();
            }
        };
        
        transport_->onReceive = [this](std::span<const std::uint8_t> data) {
            if (game_) {
                game_->on_server_message(data);
            }
        };
        
        // If transport is already connected, trigger on_connected immediately
        // (handles case where connect() was called before set_transport())
        if (transport_->is_connected() && connectionState_ != ConnectionState::Connected) {
            connectionState_ = ConnectionState::Connected;
            log(LogLevel::Info, "Connected to server (already connected)");
            if (game_) {
                game_->on_connected();
            }
        }
    }
}

void ClientEngine::run(IGameClient& game) {
    game_ = &game;
    running_ = true;
    
    // Initialize window
    init_window();
    
    // Initialize engine subsystems
    init_subsystems();
    
    // Initialize game
    game.on_init(*this);
    log(LogLevel::Info, "Client started");
    
    // Run main loop
    main_loop(game);
    
    // Shutdown
    game.on_shutdown();
    shutdown_subsystems();
    close_window();
    
    log(LogLevel::Info, "Client stopped");
    game_ = nullptr;
}

void ClientEngine::stop() {
    running_ = false;
}

// ============================================================================
// IClientServices - Networking
// ============================================================================

void ClientEngine::send(std::span<const std::uint8_t> data) {
    if (transport_ && transport_->is_connected()) {
        transport_->send(data);
    }
}

ConnectionState ClientEngine::connection_state() const {
    return connectionState_;
}

std::uint32_t ClientEngine::ping_ms() const {
    if (transport_) {
        return transport_->ping_ms();
    }
    return 0;
}

// ============================================================================
// IClientServices - World
// ============================================================================

voxel::World& ClientEngine::world() {
    if (!world_) {
        throw std::runtime_error("World not initialized - call init_world() first");
    }
    return *world_;
}

const voxel::World& ClientEngine::world() const {
    if (!world_) {
        throw std::runtime_error("World not initialized - call init_world() first");
    }
    return *world_;
}

void ClientEngine::init_world(std::uint32_t seed) {
    log(LogLevel::Info, "Initializing world with seed: " + std::to_string(seed));
    
    // Unload existing world
    if (world_) {
        world_->unload_voxel_shader();
        world_.reset();
    }
    
    // Create new world
    world_ = std::make_unique<voxel::World>(seed);
    world_->load_voxel_shader();
    
    log(LogLevel::Info, "World initialized");
}

voxel::BlockInteraction& ClientEngine::block_interaction() {
    if (!blockInteraction_) {
        throw std::runtime_error("BlockInteraction not initialized");
    }
    return *blockInteraction_;
}

// ============================================================================
// IClientServices - ECS
// ============================================================================

entt::registry& ClientEngine::registry() {
    return registry_;
}

// ============================================================================
// IClientServices - UI
// ============================================================================

ui::UIManager& ClientEngine::ui_manager() {
    if (!uiManager_) {
        throw std::runtime_error("UIManager not initialized");
    }
    return *uiManager_;
}

// ============================================================================
// IClientServices - Logging
// ============================================================================

void ClientEngine::log(LogLevel level, std::string_view msg) {
    if (!config_.logging) return;
    
    const char* prefix = "";
    switch (level) {
        case LogLevel::Debug:   prefix = "[DEBUG] "; break;
        case LogLevel::Info:    prefix = "[INFO]  "; break;
        case LogLevel::Warning: prefix = "[WARN]  "; break;
        case LogLevel::Error:   prefix = "[ERROR] "; break;
    }
    
    std::printf("%s%.*s\n", prefix, static_cast<int>(msg.size()), msg.data());
}

// ============================================================================
// Window management
// ============================================================================

void ClientEngine::init_window() {
    auto& win = rf::Window::instance();
    if (!win.init(config_.windowWidth, config_.windowHeight, config_.windowTitle, config_.vsync)) {
        log(LogLevel::Error, "Failed to create window");
        running_ = false;
        return;
    }

    // Initialize input system with the GLFW window
    rf::Input::instance().init(win.handle());

    // Set initial OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    log(LogLevel::Info, "Window initialized (GLFW + OpenGL)");
}

void ClientEngine::close_window() {
    rf::Window::instance().shutdown();
}

// ============================================================================
// Subsystem management
// ============================================================================

void ClientEngine::init_subsystems() {
    log(LogLevel::Info, "Initializing engine subsystems...");
    
    // Initialize resource system
    resources::init();
    
    // Load config
    const bool cfg_ok = core::Config::instance().load_from_file(config_.configFile);
    core::Logger::instance().init(core::Config::instance().logging());
    log(LogLevel::Info, cfg_ok ? "Config loaded" : "Config not found, using defaults");
    
    // Initialize block registry
    if (!voxel::BlockRegistry::instance().init("textures/terrain.png")) {
        log(LogLevel::Error, "Failed to initialize block registry");
    }
    
    // Initialize block model loader
    if (!voxel::BlockModelLoader::instance().init()) {
        log(LogLevel::Warning, "Failed to initialize block model loader");
    }
    
    // Initialize skybox
    renderer::Skybox::instance().init();
    
    // Initialize block interaction
    blockInteraction_ = std::make_unique<voxel::BlockInteraction>();
    if (!blockInteraction_->init()) {
        log(LogLevel::Error, "Failed to initialize block interaction");
    }
    
    // Initialize UI (UIManager owns Dear ImGui lifecycle)
    uiManager_ = std::make_unique<ui::UIManager>();
    uiManager_->init();
    
    log(LogLevel::Info, "Engine subsystems initialized");
}

void ClientEngine::shutdown_subsystems() {
    log(LogLevel::Info, "Shutting down engine subsystems...");
    
    // Shutdown UI (UIManager shuts down Dear ImGui)
    uiManager_.reset();
    
    // Shutdown block interaction
    if (blockInteraction_) {
        blockInteraction_->destroy();
        blockInteraction_.reset();
    }
    
    // Shutdown world
    if (world_) {
        world_->unload_voxel_shader();
        world_.reset();
    }
    
    // Shutdown skybox
    renderer::Skybox::instance().shutdown();
    
    log(LogLevel::Info, "Engine subsystems shut down");
}

// ============================================================================
// Main loop
// ============================================================================

void ClientEngine::main_loop(IGameClient& game) {
    auto& win = rf::Window::instance();
    auto& input = rf::Input::instance();

    while (running_ && !win.shouldClose()) {
        // Begin frame: snapshot previous input state, then poll new events
        input.beginFrame();
        win.pollEvents();

        // Compute delta time via GLFW timer
        frameDt_ = win.updateDeltaTime();

        // Poll network
        if (transport_) {
            transport_->poll(0);
        }

        // Update game logic
        game.on_update(frameDt_);

        // --- Render frame ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        game.on_render();

        win.swapBuffers();

        // Handle window resize
        if (win.isResized()) {
            config_.windowWidth = win.width();
            config_.windowHeight = win.height();
            glViewport(0, 0, win.width(), win.height());
        }
    }
}

} // namespace engine
