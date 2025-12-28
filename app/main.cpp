#include "../client/core/game.hpp"
#include "../client/core/config.hpp"

#include "../shared/transport/local_transport.hpp"
#include "../shared/transport/enet_client.hpp"
#include "../shared/transport/enet_common.hpp"
#include "../server/core/server.hpp"

#include <cstring>
#include <iostream>
#include <string>

namespace {

struct Args {
    bool connect = false;
    std::string host = "localhost";
    std::uint16_t port = 7777;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
            args.connect = true;
            std::string addr = argv[++i];
            
            // Parse host:port
            auto colon = addr.find(':');
            if (colon != std::string::npos) {
                args.host = addr.substr(0, colon);
                args.port = static_cast<std::uint16_t>(std::stoi(addr.substr(colon + 1)));
            } else {
                args.host = addr;
            }
        }
    }
    
    return args;
}

} // namespace

int main(int argc, char* argv[]) {
    Args args = parse_args(argc, argv);
    
    const bool cfg_ok = core::Config::instance().load_from_file("rayflow.conf");
    TraceLog(LOG_INFO, "[config] %s, render.voxel_smooth_lighting=%s",
             cfg_ok ? "ok" : "missing (defaults)",
             core::Config::instance().get().render.voxel_smooth_lighting ? "true" : "false");

    // Network mode: connect to remote server
    if (args.connect) {
        shared::transport::ENetInitializer enetInit;
        if (!enetInit.isInitialized()) {
            std::cerr << "[ERROR] Failed to initialize ENet\n";
            return -1;
        }
        
        std::cout << "[INFO] Connecting to " << args.host << ":" << args.port << "...\n";
        
        shared::transport::ENetClient netClient;
        if (!netClient.connect(args.host, args.port, 5000)) {
            std::cerr << "[ERROR] Failed to connect to " << args.host << ":" << args.port << "\n";
            return -1;
        }
        
        std::cout << "[INFO] Connected!\n";
        
        Game game;
        game.set_transport_endpoint(netClient.connection());
        game.set_network_client(&netClient);  // For polling
        
        if (!game.init(1280, 720, "Rayflow (bed wars) - Multiplayer")) {
            netClient.disconnect();
            return -1;
        }
        
        game.run();
        game.shutdown();
        
        netClient.disconnect();
        return 0;
    }
    
    // Local mode: embedded server
    auto pair = shared::transport::LocalTransport::create_pair();

    server::core::Server::Options sv_opts;
    {
        const auto& sv = core::Config::instance().sv_logging();
        sv_opts.logging.enabled = sv.enabled;
        sv_opts.logging.init = sv.init;
        sv_opts.logging.rx = sv.rx;
        sv_opts.logging.tx = sv.tx;
        sv_opts.logging.move = sv.move;
        sv_opts.logging.coll = sv.coll;
    }

    server::core::Server server(pair.server, sv_opts);
    server.start();

    Game game;
    game.set_transport_endpoint(pair.client);

    if (!game.init(1280, 720, "Rayflow (bed wars)")) {
        server.stop();
        return -1;
    }

    game.run();
    game.shutdown();

    server.stop();
    return 0;
}
