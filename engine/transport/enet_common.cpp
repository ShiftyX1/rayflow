#include "enet_common.hpp"

#include <enet/enet.h>
#include <cstdio>

namespace engine::transport {

ENetInitializer::ENetInitializer() {
    if (enet_initialize() == 0) {
        initialized_ = true;
    } else {
        std::fprintf(stderr, "[enet] enet_initialize() failed\n");
    }
}

ENetInitializer::~ENetInitializer() {
    if (initialized_) {
        enet_deinitialize();
    }
}

} // namespace engine::transport
