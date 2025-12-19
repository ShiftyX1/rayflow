#pragma once

/**
 * @file mock_terrain.hpp
 * @brief Deterministic mock terrain for testing physics and validation.
 * 
 * Provides a simple, predictable terrain pattern for tests that need
 * collision detection or block queries without full terrain generation.
 */

#include "voxel/block.hpp"

namespace test_helpers {

/**
 * @brief Simple deterministic terrain: solid floor at y < floor_height, air above.
 * 
 * Default: Stone floor at y=64, Air above.
 * Useful for testing gravity, ground collision, and basic movement.
 */
class MockTerrain {
public:
    explicit MockTerrain(int floor_height = 64, 
                         shared::voxel::BlockType floor_type = shared::voxel::BlockType::Stone)
        : floor_height_(floor_height), floor_type_(floor_type) {}

    shared::voxel::BlockType get_block(int x, int y, int z) const {
        (void)x;
        (void)z;
        if (y < floor_height_) {
            return floor_type_;
        }
        return shared::voxel::BlockType::Air;
    }

    bool is_solid(int x, int y, int z) const {
        return shared::voxel::util::is_solid(get_block(x, y, z));
    }

    int floor_height() const { return floor_height_; }

private:
    int floor_height_;
    shared::voxel::BlockType floor_type_;
};

/**
 * @brief Mock terrain with a configurable box of solid blocks.
 * 
 * Useful for testing wall collisions and enclosed spaces.
 */
class MockBoxTerrain {
public:
    MockBoxTerrain(int min_x, int min_y, int min_z,
                   int max_x, int max_y, int max_z,
                   shared::voxel::BlockType block_type = shared::voxel::BlockType::Stone)
        : min_x_(min_x), min_y_(min_y), min_z_(min_z)
        , max_x_(max_x), max_y_(max_y), max_z_(max_z)
        , block_type_(block_type) {}

    shared::voxel::BlockType get_block(int x, int y, int z) const {
        if (x >= min_x_ && x <= max_x_ &&
            y >= min_y_ && y <= max_y_ &&
            z >= min_z_ && z <= max_z_) {
            return block_type_;
        }
        return shared::voxel::BlockType::Air;
    }

    bool is_solid(int x, int y, int z) const {
        return shared::voxel::util::is_solid(get_block(x, y, z));
    }

private:
    int min_x_, min_y_, min_z_;
    int max_x_, max_y_, max_z_;
    shared::voxel::BlockType block_type_;
};

/**
 * @brief Mock terrain that can have blocks placed/removed programmatically.
 * 
 * Starts empty (all Air) and allows setting specific blocks.
 */
class MockEditableTerrain {
public:
    void set_block(int x, int y, int z, shared::voxel::BlockType type) {
        blocks_[key(x, y, z)] = type;
    }

    shared::voxel::BlockType get_block(int x, int y, int z) const {
        auto it = blocks_.find(key(x, y, z));
        if (it != blocks_.end()) {
            return it->second;
        }
        return shared::voxel::BlockType::Air;
    }

    bool is_solid(int x, int y, int z) const {
        return shared::voxel::util::is_solid(get_block(x, y, z));
    }

    void clear() {
        blocks_.clear();
    }

private:
    static std::uint64_t key(int x, int y, int z) {
        // Pack coordinates into a 64-bit key
        // Assumes coords fit in ~20 bits each (sufficient for testing)
        auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
        auto uy = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y));
        auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
        return (ux << 40) | (uy << 20) | uz;
    }

    std::unordered_map<std::uint64_t, shared::voxel::BlockType> blocks_;
};

} // namespace test_helpers
