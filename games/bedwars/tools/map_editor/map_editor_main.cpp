// =============================================================================
// Map Editor — Entry Point
//
// Follows the same pattern as games/bedwars/app/client_main.cpp:
//   1. Create ClientEngine + MapEditorClient
//   2. Set start_editor_callback (creates local transport + embedded server)
//   3. engine.run(game)
// =============================================================================

#include "map_editor_client.hpp"

#include "games/bedwars/server/bedwars_server.hpp"

#include "engine/core/client_engine.hpp"
#include "engine/core/server_engine.hpp"
#include "engine/transport/local_transport.hpp"
#include "engine/maps/runtime_paths.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::printf("\n");
    std::printf("==============================================\n");
    std::printf("         RAYFLOW MAP EDITOR (v2)              \n");
    std::printf("==============================================\n");
    std::printf("\n");

    // Set base path for map file discovery
    shared::maps::set_base_path(std::filesystem::current_path());

    // Shared state for embedded server
    std::shared_ptr<engine::transport::IClientTransport> clientTransport;
    std::unique_ptr<engine::ServerEngine> serverEngine;
    std::unique_ptr<bedwars::server::BedWarsServer> serverGame;
    std::thread serverThread;

    // Create client engine
    engine::ClientEngine::Config config;
    config.windowWidth  = 1280;
    config.windowHeight = 720;
    config.windowTitle  = "RayFlow Map Editor";
    config.targetFps    = 60;
    config.vsync        = true;
    config.logging      = true;

    engine::ClientEngine engine(config);

    // Create editor game client
    editor::MapEditorClient game;

    // Setup callback: when user creates/opens a map, start the embedded server
    game.set_start_editor_callback([&]() {
        std::printf("[MapEditor] Starting embedded server...\n");

        // Create local transport pair
        auto pair = engine::transport::create_local_transport_pair();
        clientTransport = pair.client;

        // Configure server
        engine::ServerEngine::Config serverConfig;
        serverConfig.tickRate = 30;
        serverConfig.logging  = true;

        serverEngine = std::make_unique<engine::ServerEngine>(serverConfig);
        serverEngine->set_transport(pair.server);

        // Create BedWars server in editor mode
        bedwars::server::BedWarsServer::Options opts;
        opts.editorCameraMode = true;
        opts.loadMapTemplate  = false;
        opts.autoStartMatch   = false;

        auto seed = static_cast<std::uint32_t>(std::time(nullptr));
        serverGame = std::make_unique<bedwars::server::BedWarsServer>(seed, opts);

        // Run server in background thread
        serverThread = std::thread([&]() {
            serverEngine->run(*serverGame);
        });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Connect client engine to the local server
        engine.set_transport(clientTransport);

        std::printf("[MapEditor] Embedded server started (seed=%u)\n", seed);
    });

    // Run
    std::printf("[MapEditor] Starting...\n");
    engine.run(game);

    // Cleanup
    if (serverEngine) {
        serverEngine->stop();
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::printf("[MapEditor] Exited cleanly\n");
    return 0;
}
