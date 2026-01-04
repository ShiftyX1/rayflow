// Example: BedWars Dedicated Server using the new Engine architecture
// This shows how clean the separation is.

#include <engine/core/server_engine.hpp>
#include <engine/transport/local_transport.hpp>
#include <games/bedwars/server/bedwars_server.hpp>

#include <csignal>
#include <iostream>

// Global for signal handler
engine::ServerEngine* g_engine = nullptr;

void signal_handler(int /*sig*/) {
    std::cout << "\nShutting down...\n";
    if (g_engine) {
        g_engine->stop();
    }
}

int main() {
    std::cout << "=== BedWars Dedicated Server (New Architecture) ===\n\n";
    
    // Setup signal handler
    std::signal(SIGINT, signal_handler);
    
    // Create engine
    engine::ServerEngine::Config config;
    config.tickRate = 30.0f;
    config.logging = true;
    
    engine::ServerEngine serverEngine(config);
    g_engine = &serverEngine;
    
    // Create transport (local for this example, would be ENet for real server)
    auto [clientTransport, serverTransport] = engine::transport::create_local_transport_pair();
    serverEngine.set_transport(serverTransport);
    
    // Create game
    bedwars::server::BedWarsServer game;
    
    // Run (blocks until stop() is called)
    serverEngine.run(game);
    
    std::cout << "Server stopped.\n";
    return 0;
}
