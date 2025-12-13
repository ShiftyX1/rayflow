#include "core/game.hpp"

int main() {
    Game game;
    
    if (!game.init(1280, 720, "RayFlow 3D Engine - ECS Architecture")) {
        return -1;
    }
    
    game.run();
    game.shutdown();
    
    return 0;
}
