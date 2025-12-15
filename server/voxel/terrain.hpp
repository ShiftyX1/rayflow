#pragma once

#include "../../shared/voxel/block.hpp"

#include "../../shared/maps/rfmap_io.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace server::voxel {

class Terrain {
public:
    explicit Terrain(std::uint32_t seed);

    std::uint32_t seed() const { return seed_; }

    shared::voxel::BlockType get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, shared::voxel::BlockType type);

    bool has_map_template() const { return map_template_.has_value(); }
    const shared::maps::MapTemplate* map_template() const { return map_template_ ? &*map_template_ : nullptr; }
    void set_map_template(shared::maps::MapTemplate map);

    // Bounds check for template editing / validation (MT-1).
    bool is_within_template_bounds(int x, int y, int z) const;

private:
    struct BlockKey {
        int x{0};
        int y{0};
        int z{0};

        bool operator==(const BlockKey& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct BlockKeyHash {
        std::size_t operator()(const BlockKey& k) const {
            // Simple hash combine; good enough for sparse overrides.
            std::size_t h = 1469598103934665603ull;
            h ^= static_cast<std::size_t>(k.x) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= static_cast<std::size_t>(k.y) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            h ^= static_cast<std::size_t>(k.z) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            return h;
        }
    };

    shared::voxel::BlockType get_base_block_(int x, int y, int z) const;
    shared::voxel::BlockType get_template_block_(int x, int y, int z) const;
    static int floor_div_(int a, int b);

    void init_perlin_() const;
    float perlin_noise_(float x, float y) const;
    float octave_perlin_(float x, float y, int octaves, float persistence) const;

    std::uint32_t seed_{0};

    std::optional<shared::maps::MapTemplate> map_template_{};

    // Sparse runtime modifications (placed/broken blocks) on top of procedural base terrain.
    std::unordered_map<BlockKey, shared::voxel::BlockType, BlockKeyHash> overrides_{};

    mutable std::array<unsigned char, 512> perm_{};
    mutable bool perm_initialized_{false};
};

} // namespace server::voxel
