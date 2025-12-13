#include "core/game.hpp"

int main() {
    Game game;
    
    if (!game.init(1280, 720, "Bed Wars")) {
        return -1;
    }
    
    game.run();
    game.shutdown();
    
    return 0;
}
