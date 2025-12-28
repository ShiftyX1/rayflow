// RFDS - RayFlow Dedicated Server
// Headless multiplayer server using ENet transport

#include "../server/core/dedicated_server.hpp"
#include "../shared/transport/enet_common.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifndef RAYFLOW_VERSION
#define RAYFLOW_VERSION "0.0.0-dev"
#endif

namespace {

volatile std::sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

void print_banner() {
    std::cout << R"(
  ____             _____ _               
 |  _ \ __ _ _   _|  ___| | _____      __
 | |_) / _` | | | | |_  | |/ _ \ \ /\ / /
 |  _ < (_| | |_| |  _| | | (_) \ V  V / 
 |_| \_\__,_|\__, |_|   |_|\___/ \_/\_/  
             |___/                       
)" << '\n';
    std::cout << "  RayFlow Dedicated Server (RFDS) v" << RAYFLOW_VERSION << "\n";
    std::cout << "  ============================================\n\n";
}

void print_usage(const char* progname) {
    std::cout << "Usage: " << progname << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --port <port>       Listen port (default: 7777)\n";
    std::cout << "  --max-players <n>   Maximum players (default: 16)\n";
    std::cout << "  --tickrate <n>      Server tick rate (default: 30)\n";
    std::cout << "  --verbose           Enable verbose logging\n";
    std::cout << "  --quiet             Disable most logging\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << progname << " --port 7777 --max-players 16\n";
}

struct Args {
    std::uint16_t port = 7777;
    std::size_t maxPlayers = 16;
    std::uint32_t tickRate = 30;
    bool verbose = false;
    bool quiet = false;
    bool help = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        
        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            args.help = true;
        }
        else if (std::strcmp(arg, "--port") == 0 && i + 1 < argc) {
            args.port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(arg, "--max-players") == 0 && i + 1 < argc) {
            args.maxPlayers = static_cast<std::size_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(arg, "--tickrate") == 0 && i + 1 < argc) {
            args.tickRate = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(arg, "--map") == 0 && i + 1 < argc) {
            ++i; // Currently unused
        }
        else if (std::strcmp(arg, "--verbose") == 0) {
            args.verbose = true;
        }
        else if (std::strcmp(arg, "--quiet") == 0) {
            args.quiet = true;
        }
        else {
            std::cerr << "[WARNING] Unknown argument: " << arg << "\n";
        }
    }
    
    return args;
}

} // namespace

int main(int argc, char* argv[]) {
    print_banner();
    
    Args args = parse_args(argc, argv);
    
    if (args.help) {
        print_usage(argv[0]);
        return 0;
    }
    
    // Initialize ENet
    shared::transport::ENetInitializer enetInit;
    if (!enetInit.isInitialized()) {
        std::cerr << "[ERROR] Failed to initialize ENet\n";
        return 1;
    }
    
    // Configure server
    server::core::DedicatedServer::Config config;
    config.port = args.port;
    config.maxClients = args.maxPlayers;
    config.tickRate = args.tickRate;
    
    if (args.quiet) {
        config.logging.enabled = false;
    } else if (args.verbose) {
        config.logging.move = true;
        config.logging.coll = true;
    }
    
    // Create and start server
    server::core::DedicatedServer server(config);
    
    server.onPlayerJoin = [](shared::proto::PlayerId id) {
        std::cout << "[INFO] Player " << id << " joined the game\n";
    };
    
    server.onPlayerLeave = [](shared::proto::PlayerId id) {
        std::cout << "[INFO] Player " << id << " left the game\n";
    };
    
    if (!server.start()) {
        std::cerr << "[ERROR] Failed to start server on port " << args.port << "\n";
        return 1;
    }
    
    std::cout << "[INFO] Server started on port " << args.port << "\n";
    std::cout << "[INFO] Max players: " << args.maxPlayers << "\n";
    std::cout << "[INFO] Tick rate: " << args.tickRate << " TPS\n";
    std::cout << "[INFO] Press Ctrl+C to stop\n\n";
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Wait for shutdown signal
    while (g_running && server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n[INFO] Shutting down...\n";
    server.stop();
    std::cout << "[INFO] Server stopped\n";
    
    return 0;
}
