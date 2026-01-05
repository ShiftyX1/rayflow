// BedWars Client Entry Point
// Usage: bedwars_client [--connect host:port] [--name PlayerName]
// Without --connect, starts in singleplayer mode with embedded server.

#include "../client/bedwars_client.hpp"
#include "../server/bedwars_server.hpp"

#include "engine/core/client_engine.hpp"
#include "engine/core/server_engine.hpp"
#include "engine/transport/local_transport.hpp"
#include "engine/transport/enet_client.hpp"
#include "engine/transport/enet_common.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

static void print_usage(const char* prog) {
    std::printf("Usage: %s [options]\n", prog);
    std::printf("Options:\n");
    std::printf("  --connect host:port   Connect to server (without this flag, starts singleplayer)\n");
    std::printf("  --name NAME           Player name (default: Player)\n");
    std::printf("  --width W             Window width (default: 1280)\n");
    std::printf("  --height H            Window height (default: 720)\n");
    std::printf("  --help                Show this help\n");
}

static void print_banner() {
    std::printf("\n");
    std::printf("╔════════════════════════════════════════════╗\n");
    std::printf("║     ____           ___       __            ║\n");
    std::printf("║    / __ )___  ____/ / |     / /___ ___     ║\n");
    std::printf("║   / __  / _ \\/ __  /| | /| / / __ `/ _ \\   ║\n");
    std::printf("║  / /_/ /  __/ /_/ / | |/ |/ / /_/ /  __/   ║\n");
    std::printf("║ /_____/\\___/\\__,_/  |__/|__/\\__,_/\\___/    ║\n");
    std::printf("║                                            ║\n");
    std::printf("║         BedWars Client (Engine v2)         ║\n");
    std::printf("╚════════════════════════════════════════════╝\n");
    std::printf("\n");
}

int main(int argc, char* argv[]) {
    // Default settings
    std::string playerName = "Player";
    int windowWidth = 1280;
    int windowHeight = 720;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            playerName = argv[++i];
        }
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            windowWidth = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            windowHeight = std::atoi(argv[++i]);
        }
    }
    
    print_banner();
    
    // Shared state for connection management
    std::shared_ptr<engine::transport::IClientTransport> clientTransport;
    std::unique_ptr<engine::transport::ENetInitializer> enetInit;
    std::unique_ptr<engine::ServerEngine> localServerEngine;
    std::unique_ptr<bedwars::server::BedWarsServer> localServerGame;
    std::thread serverThread;
    std::shared_ptr<engine::transport::ENetClientTransport> enetTransport;
    
    // Create client engine (starts in main menu, no connection yet)
    engine::ClientEngine::Config config;
    config.windowWidth = windowWidth;
    config.windowHeight = windowHeight;
    config.windowTitle = "BedWars";
    config.targetFps = 60;
    config.vsync = true;
    config.logging = true;
    
    engine::ClientEngine engine(config);
    
    // Create game
    bedwars::BedWarsClient game;
    game.set_player_name(playerName);
    
    // Setup callbacks for connection management
    game.set_start_singleplayer_callback([&]() {
        std::printf("[INFO] Starting singleplayer...\n");
        
        // Create LocalTransport pair
        auto pair = engine::transport::create_local_transport_pair();
        clientTransport = pair.client;
        
        // Create and start embedded server
        engine::ServerEngine::Config serverConfig;
        serverConfig.tickRate = 30;
        serverConfig.logging = true;
        
        localServerEngine = std::make_unique<engine::ServerEngine>(serverConfig);
        localServerEngine->set_transport(pair.server);
        
        localServerGame = std::make_unique<bedwars::server::BedWarsServer>();
        
        // Run server in background thread
        serverThread = std::thread([&]() {
            localServerEngine->run(*localServerGame);
        });
        
        // Wait a moment for server to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Set transport on engine and trigger connection
        engine.set_transport(clientTransport);
        clientTransport->poll(0);
        
        std::printf("[INFO] Embedded server started.\n");
    });
    
    game.set_connect_multiplayer_callback([&](const std::string& host, std::uint16_t port) -> bool {
        std::printf("[INFO] Connecting to %s:%u...\n", host.c_str(), port);
        
        // Initialize ENet if needed
        if (!enetInit) {
            enetInit = std::make_unique<engine::transport::ENetInitializer>();
            if (!enetInit->is_initialized()) {
                std::fprintf(stderr, "[ERROR] Failed to initialize ENet\n");
                return false;
            }
        }
        
        // Create ENet transport and connect
        enetTransport = std::make_shared<engine::transport::ENetClientTransport>();
        
        if (!enetTransport->connect(host, port, 5000)) {
            std::fprintf(stderr, "[ERROR] Failed to connect to %s:%u\n", host.c_str(), port);
            enetTransport.reset();
            return false;
        }
        
        clientTransport = enetTransport;
        engine.set_transport(clientTransport);
        
        std::printf("[INFO] Connected to %s:%u\n", host.c_str(), port);
        return true;
    });
    
    game.set_disconnect_callback([&]() {
        std::printf("[INFO] Disconnecting...\n");
        
        // Disconnect ENet transport
        if (enetTransport) {
            enetTransport->disconnect();
            enetTransport.reset();
        }
        
        // Stop embedded server
        if (localServerEngine) {
            localServerEngine->stop();
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
        localServerEngine.reset();
        localServerGame.reset();
        
        clientTransport.reset();
        engine.set_transport(nullptr);
        
        std::printf("[INFO] Disconnected.\n");
    });
    
    // Run (starts in main menu)
    std::printf("[INFO] Starting game...\n");
    engine.run(game);
    
    // Cleanup on exit
    if (enetTransport) {
        enetTransport->disconnect();
    }
    if (localServerEngine) {
        localServerEngine->stop();
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    std::printf("[INFO] Client exited cleanly\n");
    return 0;
}
