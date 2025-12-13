#include "../client/core/game.hpp"

#include "../shared/transport/local_transport.hpp"
#include "../server/core/server.hpp"

int main() {
    auto pair = shared::transport::LocalTransport::create_pair();

    server::core::Server server(pair.server);
    server.start();

    Game game;
    game.set_transport_endpoint(pair.client);

    if (!game.init(1280, 720, "RayFlow 3D Engine - ECS Architecture")) {
        server.stop();
        return -1;
    }

    game.run();
    game.shutdown();

    server.stop();
    return 0;
}
