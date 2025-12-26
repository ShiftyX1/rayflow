#include "../client/core/game.hpp"
#include "../client/core/config.hpp"

#include "../shared/transport/local_transport.hpp"
#include "../server/core/server.hpp"

int main() {
    const bool cfg_ok = core::Config::instance().load_from_file("rayflow.conf");
    TraceLog(LOG_INFO, "[config] %s, render.voxel_smooth_lighting=%s",
             cfg_ok ? "ok" : "missing (defaults)",
             core::Config::instance().get().render.voxel_smooth_lighting ? "true" : "false");

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
