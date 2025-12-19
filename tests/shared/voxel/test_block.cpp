/**
 * @file test_block.cpp
 * @brief Unit tests for block types and utilities.
 * 
 * Tests enum stability, light properties, and utility functions.
 */

#include <catch2/catch_test_macros.hpp>

#include "voxel/block.hpp"

using namespace shared::voxel;

// =============================================================================
// BlockType enum stability tests
// =============================================================================

TEST_CASE("BlockType enum values are stable", "[voxel][enum]") {
    // These values must never change (save file compatibility, protocol)
    REQUIRE(static_cast<int>(BlockType::Air) == 0);
    REQUIRE(static_cast<int>(BlockType::Stone) == 1);
    REQUIRE(static_cast<int>(BlockType::Dirt) == 2);
    REQUIRE(static_cast<int>(BlockType::Grass) == 3);
    REQUIRE(static_cast<int>(BlockType::Sand) == 4);
    REQUIRE(static_cast<int>(BlockType::Water) == 5);
    REQUIRE(static_cast<int>(BlockType::Wood) == 6);
    REQUIRE(static_cast<int>(BlockType::Leaves) == 7);
    REQUIRE(static_cast<int>(BlockType::Bedrock) == 8);
    REQUIRE(static_cast<int>(BlockType::Gravel) == 9);
    REQUIRE(static_cast<int>(BlockType::Coal) == 10);
    REQUIRE(static_cast<int>(BlockType::Iron) == 11);
    REQUIRE(static_cast<int>(BlockType::Gold) == 12);
    REQUIRE(static_cast<int>(BlockType::Diamond) == 13);
    REQUIRE(static_cast<int>(BlockType::Light) == 14);
    REQUIRE(static_cast<int>(BlockType::Count) == 15);
}

// =============================================================================
// Chunk dimension tests
// =============================================================================

TEST_CASE("Chunk dimensions are correct", "[voxel][chunk]") {
    REQUIRE(CHUNK_WIDTH == 16);
    REQUIRE(CHUNK_HEIGHT == 256);
    REQUIRE(CHUNK_DEPTH == 16);
    REQUIRE(CHUNK_SIZE == CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH);
    REQUIRE(CHUNK_SIZE == 65536);
}

// =============================================================================
// Light properties array tests
// =============================================================================

TEST_CASE("BLOCK_LIGHT_PROPS array size matches BlockType::Count", "[voxel][light]") {
    constexpr auto array_size = sizeof(BLOCK_LIGHT_PROPS) / sizeof(BLOCK_LIGHT_PROPS[0]);
    REQUIRE(array_size == static_cast<std::size_t>(BlockType::Count));
}

TEST_CASE("get_light_props returns valid reference", "[voxel][light]") {
    // Just verify no crash and returns reference for each type
    for (int i = 0; i < static_cast<int>(BlockType::Count); ++i) {
        auto bt = static_cast<BlockType>(i);
        const auto& props = get_light_props(bt);
        // Emission is in valid range
        REQUIRE(props.emission <= 15);
    }
}

TEST_CASE("Air has no light emission and is not opaque", "[voxel][light]") {
    const auto& props = get_light_props(BlockType::Air);
    REQUIRE(props.emission == 0);
    REQUIRE_FALSE(props.opaqueForLight);
    REQUIRE_FALSE(props.skyDimVertical);
}

TEST_CASE("Light block emits maximum light", "[voxel][light]") {
    const auto& props = get_light_props(BlockType::Light);
    REQUIRE(props.emission == 15);
    REQUIRE_FALSE(props.opaqueForLight);
}

TEST_CASE("Opaque blocks block light propagation", "[voxel][light]") {
    // Stone, Dirt, Bedrock etc. should be opaque
    REQUIRE(get_light_props(BlockType::Stone).opaqueForLight);
    REQUIRE(get_light_props(BlockType::Dirt).opaqueForLight);
    REQUIRE(get_light_props(BlockType::Bedrock).opaqueForLight);
    REQUIRE(get_light_props(BlockType::Wood).opaqueForLight);
}

TEST_CASE("Transparent blocks allow light propagation", "[voxel][light]") {
    REQUIRE_FALSE(get_light_props(BlockType::Air).opaqueForLight);
    REQUIRE_FALSE(get_light_props(BlockType::Water).opaqueForLight);
    REQUIRE_FALSE(get_light_props(BlockType::Leaves).opaqueForLight);
    REQUIRE_FALSE(get_light_props(BlockType::Light).opaqueForLight);
}

TEST_CASE("Leaves and Water dim sky light vertically", "[voxel][light]") {
    REQUIRE(get_light_props(BlockType::Leaves).skyDimVertical);
    REQUIRE(get_light_props(BlockType::Water).skyDimVertical);
    
    // Air and solid blocks don't dim sky vertically (solid blocks are opaque anyway)
    REQUIRE_FALSE(get_light_props(BlockType::Air).skyDimVertical);
}

// =============================================================================
// is_solid utility tests
// =============================================================================

TEST_CASE("is_solid returns false for Air", "[voxel][util]") {
    REQUIRE_FALSE(util::is_solid(BlockType::Air));
}

TEST_CASE("is_solid returns false for Water", "[voxel][util]") {
    REQUIRE_FALSE(util::is_solid(BlockType::Water));
}

TEST_CASE("is_solid returns false for Light", "[voxel][util]") {
    REQUIRE_FALSE(util::is_solid(BlockType::Light));
}

TEST_CASE("is_solid returns true for solid blocks", "[voxel][util]") {
    REQUIRE(util::is_solid(BlockType::Stone));
    REQUIRE(util::is_solid(BlockType::Dirt));
    REQUIRE(util::is_solid(BlockType::Grass));
    REQUIRE(util::is_solid(BlockType::Sand));
    REQUIRE(util::is_solid(BlockType::Wood));
    REQUIRE(util::is_solid(BlockType::Leaves));
    REQUIRE(util::is_solid(BlockType::Bedrock));
    REQUIRE(util::is_solid(BlockType::Gravel));
    REQUIRE(util::is_solid(BlockType::Coal));
    REQUIRE(util::is_solid(BlockType::Iron));
    REQUIRE(util::is_solid(BlockType::Gold));
    REQUIRE(util::is_solid(BlockType::Diamond));
}

// =============================================================================
// is_transparent utility tests
// =============================================================================

TEST_CASE("is_transparent returns true for Air", "[voxel][util]") {
    REQUIRE(util::is_transparent(BlockType::Air));
}

TEST_CASE("is_transparent returns true for Water", "[voxel][util]") {
    REQUIRE(util::is_transparent(BlockType::Water));
}

TEST_CASE("is_transparent returns true for Leaves", "[voxel][util]") {
    REQUIRE(util::is_transparent(BlockType::Leaves));
}

TEST_CASE("is_transparent returns true for Light", "[voxel][util]") {
    REQUIRE(util::is_transparent(BlockType::Light));
}

TEST_CASE("is_transparent returns false for opaque blocks", "[voxel][util]") {
    REQUIRE_FALSE(util::is_transparent(BlockType::Stone));
    REQUIRE_FALSE(util::is_transparent(BlockType::Dirt));
    REQUIRE_FALSE(util::is_transparent(BlockType::Grass));
    REQUIRE_FALSE(util::is_transparent(BlockType::Sand));
    REQUIRE_FALSE(util::is_transparent(BlockType::Wood));
    REQUIRE_FALSE(util::is_transparent(BlockType::Bedrock));
    REQUIRE_FALSE(util::is_transparent(BlockType::Coal));
    REQUIRE_FALSE(util::is_transparent(BlockType::Iron));
    REQUIRE_FALSE(util::is_transparent(BlockType::Gold));
    REQUIRE_FALSE(util::is_transparent(BlockType::Diamond));
}

// =============================================================================
// Consistency tests
// =============================================================================

TEST_CASE("Solid and transparent are not mutually exclusive", "[voxel][util]") {
    // Leaves is solid (for collision) but transparent (for rendering)
    REQUIRE(util::is_solid(BlockType::Leaves));
    REQUIRE(util::is_transparent(BlockType::Leaves));
}

TEST_CASE("Air is neither solid nor opaque", "[voxel][util]") {
    REQUIRE_FALSE(util::is_solid(BlockType::Air));
    REQUIRE(util::is_transparent(BlockType::Air));
}
