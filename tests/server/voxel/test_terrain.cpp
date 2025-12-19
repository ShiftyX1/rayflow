/**
 * @file test_terrain.cpp
 * @brief Unit tests for server terrain generation.
 */

#include <catch2/catch_test_macros.hpp>

#include "voxel/terrain.hpp"
#include "voxel/block.hpp"

using namespace server::voxel;
using namespace shared::voxel;

// =============================================================================
// Terrain construction tests
// =============================================================================

TEST_CASE("Terrain can be constructed with seed", "[server][terrain]") {
    Terrain terrain(12345u);
    // No crash
}

TEST_CASE("Terrain can be constructed with default seed", "[server][terrain]") {
    Terrain terrain(0u);
    // No crash
}

// =============================================================================
// Determinism tests
// =============================================================================

TEST_CASE("Terrain generation is deterministic", "[server][terrain][determinism]") {
    constexpr std::uint32_t SEED = 42u;
    
    Terrain terrain1(SEED);
    Terrain terrain2(SEED);
    
    // Same seed should produce same blocks at various positions
    for (int x = -10; x <= 10; x += 5) {
        for (int z = -10; z <= 10; z += 5) {
            for (int y = 0; y < 100; y += 10) {
                auto block1 = terrain1.get_block(x, y, z);
                auto block2 = terrain2.get_block(x, y, z);
                REQUIRE(block1 == block2);
            }
        }
    }
}

TEST_CASE("Different seeds produce different terrain", "[server][terrain][determinism]") {
    Terrain terrain1(100u);
    Terrain terrain2(200u);
    
    // Different seeds should (very likely) produce different terrain
    int differences = 0;
    for (int x = 0; x < 10; ++x) {
        for (int z = 0; z < 10; ++z) {
            int y = 64;  // Surface level
            if (terrain1.get_block(x, y, z) != terrain2.get_block(x, y, z)) {
                differences++;
            }
        }
    }
    
    // At least some blocks should differ
    REQUIRE(differences > 0);
}

// =============================================================================
// Block access tests
// =============================================================================

TEST_CASE("Terrain get_block returns valid block types", "[server][terrain]") {
    Terrain terrain(12345u);
    
    // Check various positions
    for (int y = 0; y < 100; ++y) {
        auto block = terrain.get_block(0, y, 0);
        int blockInt = static_cast<int>(block);
        REQUIRE(blockInt >= 0);
        REQUIRE(blockInt < static_cast<int>(BlockType::Count));
    }
}

TEST_CASE("Terrain has solid blocks at low Y", "[server][terrain]") {
    Terrain terrain(12345u);
    
    // Very low Y should generally be solid (stone, bedrock, etc.)
    int solidCount = 0;
    for (int x = 0; x < 10; ++x) {
        for (int z = 0; z < 10; ++z) {
            if (util::is_solid(terrain.get_block(x, 1, z))) {
                solidCount++;
            }
        }
    }
    
    // Most blocks at y=1 should be solid
    REQUIRE(solidCount > 50);  // >50% of 100 samples
}

TEST_CASE("Terrain has air at high Y", "[server][terrain]") {
    Terrain terrain(12345u);
    
    // Very high Y should be air
    int airCount = 0;
    for (int x = 0; x < 10; ++x) {
        for (int z = 0; z < 10; ++z) {
            if (terrain.get_block(x, 200, z) == BlockType::Air) {
                airCount++;
            }
        }
    }
    
    // All blocks at y=200 should be air
    REQUIRE(airCount == 100);
}

// =============================================================================
// Block modification tests
// =============================================================================

TEST_CASE("Terrain set_block persists", "[server][terrain]") {
    Terrain terrain(12345u);
    
    int x = 50, y = 64, z = 50;
    
    // Set a block
    terrain.set_block(x, y, z, BlockType::Diamond);
    
    // Read it back
    REQUIRE(terrain.get_block(x, y, z) == BlockType::Diamond);
}

TEST_CASE("Terrain set_block overwrites existing", "[server][terrain]") {
    Terrain terrain(12345u);
    
    int x = 50, y = 64, z = 50;
    
    // Set, then overwrite
    terrain.set_block(x, y, z, BlockType::Stone);
    REQUIRE(terrain.get_block(x, y, z) == BlockType::Stone);
    
    terrain.set_block(x, y, z, BlockType::Air);
    REQUIRE(terrain.get_block(x, y, z) == BlockType::Air);
}

TEST_CASE("Terrain set_block doesn't affect neighbors", "[server][terrain]") {
    Terrain terrain(12345u);
    
    int x = 50, y = 64, z = 50;
    
    // Record neighbor blocks
    auto above = terrain.get_block(x, y+1, z);
    auto below = terrain.get_block(x, y-1, z);
    auto east = terrain.get_block(x+1, y, z);
    auto west = terrain.get_block(x-1, y, z);
    
    // Set center block
    terrain.set_block(x, y, z, BlockType::Gold);
    
    // Neighbors unchanged
    REQUIRE(terrain.get_block(x, y+1, z) == above);
    REQUIRE(terrain.get_block(x, y-1, z) == below);
    REQUIRE(terrain.get_block(x+1, y, z) == east);
    REQUIRE(terrain.get_block(x-1, y, z) == west);
}

// =============================================================================
// Boundary tests
// =============================================================================

TEST_CASE("Terrain handles negative coordinates", "[server][terrain]") {
    Terrain terrain(12345u);
    
    // Should not crash and return valid block
    auto block = terrain.get_block(-100, 64, -100);
    int blockInt = static_cast<int>(block);
    REQUIRE(blockInt >= 0);
    REQUIRE(blockInt < static_cast<int>(BlockType::Count));
}

TEST_CASE("Terrain handles zero Y coordinate", "[server][terrain]") {
    Terrain terrain(12345u);
    
    // y=0 is typically bedrock layer
    auto block = terrain.get_block(0, 0, 0);
    // Should be solid at bottom
    REQUIRE(util::is_solid(block));
}

TEST_CASE("Terrain handles Y beyond chunk height", "[server][terrain]") {
    Terrain terrain(12345u);
    
    // Beyond max height should return Air
    auto block = terrain.get_block(0, 300, 0);
    REQUIRE(block == BlockType::Air);
}
