// BedWars Dedicated Server using new Engine architecture
// Full network server using ENet transport

#include <engine/core/server_engine.hpp>
#include <engine/transport/enet_server.hpp>
#include "../server/bedwars_server.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#ifndef BEDWARS_VERSION
#define BEDWARS_VERSION "v0.1.0"
#endif

namespace {

volatile std::sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

void print_banner() {
    std::cout << R"(
  ____           ___          __              
 | __ )  ___  __| \ \        / /_ _ _ __ ___ 
 |  _ \ / _ \/ _` |\ \  /\  / / _` | '__/ __|
 | |_) |  __/ (_| | \ \/  \/ / (_| | |  \__ \
 |____/ \___|\__,_|  \_/\_/\_/ \__,_|_|  |___/
                                              
)" << '\n';
    std::cout << "  BedWars Server " << BEDWARS_VERSION << " (Engine Architecture)\n";
    std::cout << "  ================================================\n\n";
}

void print_usage(const char* progname) {
    std::cout << "Usage: " << progname << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --port <port>       Listen port (default: 7777)\n";
    std::cout << "  --max-players <n>   Maximum players (default: 16)\n";
    std::cout << "  --tickrate <n>      Server tick rate (default: 30)\n";
    std::cout << "  --seed <n>          World seed (default: 12345)\n";
    std::cout << "  --map <name>        Map file to load (default: most recent)\n";
    std::cout << "  --editor            Enable editor camera mode\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << progname << " --port 7777 --map arena.rfmap\n";
}

struct Args {
    std::uint16_t port = 7777;
    std::size_t maxPlayers = 16;
    std::uint32_t tickRate = 30;
    std::uint32_t seed = 12345;
    std::string mapName;
    bool editorMode = false;
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
        else if (std::strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            args.seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(arg, "--map") == 0 && i + 1 < argc) {
            args.mapName = argv[++i];
        }
        else if (std::strcmp(arg, "--editor") == 0) {
            args.editorMode = true;
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
    
    // Create ENet server transport
    auto transport = std::make_shared<engine::transport::ENetServerTransport>();
    
    if (!transport->start(args.port, args.maxPlayers)) {
        std::cerr << "[ERROR] Failed to start server on port " << args.port << "\n";
        return 1;
    }
    
    // Create game server with options
    bedwars::server::BedWarsServer::Options opts;
    opts.editorCameraMode = args.editorMode;
    opts.autoStartMatch = !args.editorMode;  // Don't auto-start in editor mode
    opts.mapName = args.mapName;
    
    bedwars::server::BedWarsServer game(args.seed, opts);
    
    // Create and configure engine
    engine::ServerEngine::Config config;
    config.tickRate = static_cast<float>(args.tickRate);
    
    engine::ServerEngine engine(config);
    engine.set_transport(transport);
    
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "[INFO] Server started on port " << args.port << "\n";
    std::cout << "[INFO] Max players: " << args.maxPlayers << "\n";
    std::cout << "[INFO] Tick rate: " << args.tickRate << " TPS\n";
    std::cout << "[INFO] World seed: " << args.seed << "\n";
    if (args.editorMode) {
        std::cout << "[INFO] Editor mode: ENABLED\n";
    }
    std::cout << "[INFO] Press Ctrl+C to stop\n\n";
    
    // Run in background thread so we can check g_running
    std::thread serverThread([&]() {
        engine.run(game);
    });
    
    // Main wait loop
    while (g_running && transport->is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n[INFO] Shutting down...\n";
    engine.stop();
    transport->stop();
    serverThread.join();
    std::cout << "[INFO] Server stopped\n";
    
    return 0;
}
