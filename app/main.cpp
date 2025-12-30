#include "../client/core/game.hpp"
#include "../client/core/config.hpp"

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

    Game game;

    if (!game.init(1280, 720, "Rayflow (bed wars)")) {
        return -1;
    }

    // If --connect was specified, auto-connect via menu system
    // (The game now handles connections through its menu system)
    // Legacy CLI connect support removed - use in-game menu

    game.run();
    game.shutdown();

    return 0;
}
