#pragma once

#include "../../shared/voxel/block.hpp"

#include <array>
#include <cstdint>

namespace server::voxel {

class Terrain {
public:
    explicit Terrain(std::uint32_t seed);

    std::uint32_t seed() const { return seed_; }

    shared::voxel::BlockType get_block(int x, int y, int z) const;

private:
    void init_perlin_() const;
    float perlin_noise_(float x, float y) const;
    float octave_perlin_(float x, float y, int octaves, float persistence) const;

    std::uint32_t seed_{0};

    mutable std::array<unsigned char, 512> perm_{};
    mutable bool perm_initialized_{false};
};

} // namespace server::voxel
