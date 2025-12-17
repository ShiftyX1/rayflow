#pragma once

#include "../voxel/block.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

namespace shared::maps {

struct ChunkBounds {
    std::int32_t chunkMinX{0};
    std::int32_t chunkMinZ{0};
    std::int32_t chunkMaxX{0};
    std::int32_t chunkMaxZ{0};
};

using BlockGetter = std::function<::shared::voxel::BlockType(int x, int y, int z)>;

struct MapTemplate {
    // MV-1: render-only environment settings. Must be persisted in .rfmap (format v2+ section table).
    enum class SkyboxKind : std::uint8_t {
        None = 0,
        Day = 1,
        Night = 2,
    };

    struct VisualSettings {
        // MV-1 defined 0=None, 1=Day, 2=Night. Extended: values >2 select Panorama_Sky_XX by numeric id.
        SkyboxKind skyboxKind{SkyboxKind::Day};
        float timeOfDayHours{12.0f};
        bool useMoon{false};
        float sunIntensity{1.0f};
        float ambientIntensity{0.25f};

        // MV-2 (visual-only): global temperature used for foliage/grass tint.
        // Range: [0, 1] where 0=cold, 1=hot.
        float temperature{0.5f};
    };

    struct ChunkCoordHash {
        std::size_t operator()(const std::pair<std::int32_t, std::int32_t>& coord) const {
            // Basic combine; stable and good enough.
            std::size_t h1 = std::hash<std::int32_t>{}(coord.first);
            std::size_t h2 = std::hash<std::int32_t>{}(coord.second);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

    struct ChunkData {
        std::array<::shared::voxel::BlockType, ::shared::voxel::CHUNK_SIZE> blocks{};
    };

    std::string mapId;
    std::uint32_t version{0};

    // Finite bounds in chunks (inclusive).
    ChunkBounds bounds;

    // World boundary rule (MT-1). For now it's chunk AABB (inclusive).
    ChunkBounds worldBoundary;

    // Template protection metadata (MT-1): allow-list of template blocks that may be broken.
    // Defaults to all false.
    std::array<bool, static_cast<std::size_t>(::shared::voxel::BlockType::Count)> breakableTemplateBlocks{};

    // Sparse set of chunks that contain any non-Air blocks.
    std::unordered_map<std::pair<std::int32_t, std::int32_t>, ChunkData, ChunkCoordHash> chunks;

    // MV-1: render-only visual settings.
    VisualSettings visualSettings{};

    const ChunkData* find_chunk(std::int32_t cx, std::int32_t cz) const {
        const auto it = chunks.find({cx, cz});
        return (it == chunks.end()) ? nullptr : &it->second;
    }
};

inline MapTemplate::VisualSettings default_visual_settings() {
    // MV-1 defaults when section is missing.
    MapTemplate::VisualSettings s{};
    s.skyboxKind = MapTemplate::SkyboxKind::Day;
    s.timeOfDayHours = 12.0f;
    s.useMoon = false;
    s.sunIntensity = 1.0f;
    s.ambientIntensity = 0.25f;
    s.temperature = 0.5f;
    return s;
}

struct ExportRequest {
    std::string mapId;
    std::uint32_t version{0};
    ChunkBounds bounds;

    // MT-1: template protection metadata to embed in the exported template.
    // Defaults to all false.
    std::array<bool, static_cast<std::size_t>(::shared::voxel::BlockType::Count)> breakableTemplateBlocks{};

    // MV-1: render-only environment settings to embed in the exported template.
    MapTemplate::VisualSettings visualSettings{default_visual_settings()};
};

// Writes a sparse `.rfmap` template.
//
// MT-1:
// - stores only non-Air blocks
// - bounds are configured in chunks
// - chunk records store local (lx,ly,lz) and blockType
//
// On failure returns false and fills outError (if provided).
bool write_rfmap(const std::filesystem::path& path,
                const ExportRequest& req,
                const BlockGetter& getBlock,
                std::string* outError);

// Reads a `.rfmap` template from disk.
// On failure returns false and fills outError (if provided).
bool read_rfmap(const std::filesystem::path& path,
               MapTemplate* outMap,
               std::string* outError);

} // namespace shared::maps
