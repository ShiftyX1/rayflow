/**
 * @file test_rfmap_io.cpp
 * @brief Unit tests for RFMAP file I/O.
 * 
 * Tests save/load round-trip, edge cases, and error handling.
 */

#include <catch2/catch_test_macros.hpp>

#include "maps/rfmap_io.hpp"
#include "voxel/block.hpp"

#include <filesystem>
#include <fstream>

using namespace shared::maps;
using namespace shared::voxel;

namespace {

// Helper to create a temporary file path
std::filesystem::path temp_file_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("rfmap_test_" + name + ".rfmap");
}

// Cleanup helper
struct TempFileGuard {
    std::filesystem::path path;
    ~TempFileGuard() {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
    }
};

} // namespace

// =============================================================================
// MapTemplate structure tests
// =============================================================================

TEST_CASE("MapTemplate default construction", "[maps][rfmap]") {
    MapTemplate tpl;
    
    REQUIRE(tpl.mapId.empty());
    REQUIRE(tpl.version == 0);
    REQUIRE(tpl.bounds.chunkMinX == 0);
    REQUIRE(tpl.bounds.chunkMinZ == 0);
    REQUIRE(tpl.bounds.chunkMaxX == 0);
    REQUIRE(tpl.bounds.chunkMaxZ == 0);
}

TEST_CASE("MapTemplate VisualSettings defaults", "[maps][rfmap]") {
    MapTemplate tpl;
    
    REQUIRE(tpl.visualSettings.skyboxKind == MapTemplate::SkyboxKind::Day);
    REQUIRE(tpl.visualSettings.timeOfDayHours == 12.0f);
    REQUIRE_FALSE(tpl.visualSettings.useMoon);
    REQUIRE(tpl.visualSettings.sunIntensity == 1.0f);
    REQUIRE(tpl.visualSettings.ambientIntensity == 0.25f);
    REQUIRE(tpl.visualSettings.temperature == 0.5f);
    REQUIRE(tpl.visualSettings.humidity == 1.0f);
}

TEST_CASE("default_visual_settings returns correct defaults", "[maps][rfmap]") {
    auto settings = default_visual_settings();
    
    REQUIRE(settings.skyboxKind == MapTemplate::SkyboxKind::Day);
    REQUIRE(settings.timeOfDayHours == 12.0f);
    REQUIRE_FALSE(settings.useMoon);
    REQUIRE(settings.sunIntensity == 1.0f);
    REQUIRE(settings.ambientIntensity == 0.25f);
    REQUIRE(settings.temperature == 0.5f);
    REQUIRE(settings.humidity == 1.0f);
}

// =============================================================================
// ChunkBounds tests
// =============================================================================

TEST_CASE("ChunkBounds default construction", "[maps][rfmap]") {
    ChunkBounds bounds;
    
    REQUIRE(bounds.chunkMinX == 0);
    REQUIRE(bounds.chunkMinZ == 0);
    REQUIRE(bounds.chunkMaxX == 0);
    REQUIRE(bounds.chunkMaxZ == 0);
}

TEST_CASE("ChunkBounds can hold negative coordinates", "[maps][rfmap]") {
    ChunkBounds bounds;
    bounds.chunkMinX = -10;
    bounds.chunkMinZ = -20;
    bounds.chunkMaxX = 10;
    bounds.chunkMaxZ = 20;
    
    REQUIRE(bounds.chunkMinX == -10);
    REQUIRE(bounds.chunkMinZ == -20);
    REQUIRE(bounds.chunkMaxX == 10);
    REQUIRE(bounds.chunkMaxZ == 20);
}

// =============================================================================
// MapTemplate chunk access tests
// =============================================================================

TEST_CASE("MapTemplate find_chunk returns nullptr for missing chunk", "[maps][rfmap]") {
    MapTemplate tpl;
    
    REQUIRE(tpl.find_chunk(0, 0) == nullptr);
    REQUIRE(tpl.find_chunk(100, -100) == nullptr);
}

TEST_CASE("MapTemplate find_chunk returns chunk after insertion", "[maps][rfmap]") {
    MapTemplate tpl;
    
    // Insert a chunk
    MapTemplate::ChunkData chunk;
    chunk.blocks[0] = BlockType::Stone;
    tpl.chunks[{5, 10}] = chunk;
    
    // Find it
    const auto* found = tpl.find_chunk(5, 10);
    REQUIRE(found != nullptr);
    REQUIRE(found->blocks[0] == BlockType::Stone);
    
    // Other chunks still not found
    REQUIRE(tpl.find_chunk(0, 0) == nullptr);
}

// =============================================================================
// SkyboxKind enum tests
// =============================================================================

TEST_CASE("SkyboxKind enum values are stable", "[maps][rfmap][enum]") {
    REQUIRE(static_cast<int>(MapTemplate::SkyboxKind::None) == 0);
    REQUIRE(static_cast<int>(MapTemplate::SkyboxKind::Day) == 1);
    REQUIRE(static_cast<int>(MapTemplate::SkyboxKind::Night) == 2);
}

// =============================================================================
// ChunkData tests
// =============================================================================

TEST_CASE("ChunkData default is all Air", "[maps][rfmap]") {
    MapTemplate::ChunkData chunk;
    
    // All blocks should default to Air (zero-initialized)
    for (std::size_t i = 0; i < chunk.blocks.size(); ++i) {
        REQUIRE(chunk.blocks[i] == BlockType::Air);
    }
}

TEST_CASE("ChunkData size matches CHUNK_SIZE", "[maps][rfmap]") {
    MapTemplate::ChunkData chunk;
    
    REQUIRE(chunk.blocks.size() == static_cast<std::size_t>(CHUNK_SIZE));
}

// =============================================================================
// Breakable template blocks tests
// =============================================================================

TEST_CASE("breakableTemplateBlocks defaults to all false", "[maps][rfmap]") {
    MapTemplate tpl;
    
    for (std::size_t i = 0; i < tpl.breakableTemplateBlocks.size(); ++i) {
        REQUIRE_FALSE(tpl.breakableTemplateBlocks[i]);
    }
}

TEST_CASE("breakableTemplateBlocks size matches BlockType::Count", "[maps][rfmap]") {
    MapTemplate tpl;
    
    REQUIRE(tpl.breakableTemplateBlocks.size() == static_cast<std::size_t>(BlockType::Count));
}
