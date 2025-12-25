#include "rfmap_io.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace shared::maps {

namespace {

constexpr std::array<unsigned char, 4> kMagic{{'R', 'F', 'M', 'P'}};
constexpr std::uint32_t kFormatVersion = 2;

constexpr std::uint32_t make_tag(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 0) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

constexpr std::uint32_t kSectionTagVisualSettings = make_tag('V', 'I', 'S', '0');
constexpr std::uint32_t kVisualSettingsPayloadMinSize = 16; // MV-1 payload prefix
constexpr std::uint32_t kVisualSettingsPayloadSizeV2 = 20;  // MV-2 payload (adds temperature)
constexpr std::uint32_t kVisualSettingsPayloadSize = 24;    // MV-3 payload (adds humidity)

// MT-1: template protection allow-list by BlockType id.
constexpr std::uint32_t kSectionTagProtection = make_tag('P', 'R', 'O', '0');
constexpr std::uint32_t kProtectionPayloadSize = static_cast<std::uint32_t>(static_cast<std::size_t>(::shared::voxel::BlockType::Count));

bool read_bytes(std::ifstream& in, void* data, std::size_t size) {
    in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(in);
}

// Generic istream version for reading from memory.
bool read_bytes(std::istream& in, void* data, std::size_t size) {
    in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(in);
}

bool read_u8(std::ifstream& in, std::uint8_t* out) {
    return read_bytes(in, out, sizeof(*out));
}

bool read_u8(std::istream& in, std::uint8_t* out) {
    return read_bytes(in, out, sizeof(*out));
}

bool read_u16_le(std::ifstream& in, std::uint16_t* out) {
    unsigned char b[2] = {0, 0};
    if (!read_bytes(in, b, sizeof(b))) return false;
    *out = static_cast<std::uint16_t>(static_cast<std::uint16_t>(b[0]) | (static_cast<std::uint16_t>(b[1]) << 8));
    return true;
}

bool read_u16_le(std::istream& in, std::uint16_t* out) {
    unsigned char b[2] = {0, 0};
    if (!read_bytes(in, b, sizeof(b))) return false;
    *out = static_cast<std::uint16_t>(static_cast<std::uint16_t>(b[0]) | (static_cast<std::uint16_t>(b[1]) << 8));
    return true;
}

bool read_u32_le(std::ifstream& in, std::uint32_t* out) {
    unsigned char b[4] = {0, 0, 0, 0};
    if (!read_bytes(in, b, sizeof(b))) return false;
    *out = static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(b[0]) << 0) |
        (static_cast<std::uint32_t>(b[1]) << 8) |
        (static_cast<std::uint32_t>(b[2]) << 16) |
        (static_cast<std::uint32_t>(b[3]) << 24));
    return true;
}

bool read_u32_le(std::istream& in, std::uint32_t* out) {
    unsigned char b[4] = {0, 0, 0, 0};
    if (!read_bytes(in, b, sizeof(b))) return false;
    *out = static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(b[0]) << 0) |
        (static_cast<std::uint32_t>(b[1]) << 8) |
        (static_cast<std::uint32_t>(b[2]) << 16) |
        (static_cast<std::uint32_t>(b[3]) << 24));
    return true;
}

bool read_f32_le(std::ifstream& in, float* out) {
    std::uint32_t u = 0;
    if (!read_u32_le(in, &u)) return false;
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::memcpy(out, &u, sizeof(float));
    return true;
}

bool read_f32_le(std::istream& in, float* out) {
    std::uint32_t u = 0;
    if (!read_u32_le(in, &u)) return false;
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::memcpy(out, &u, sizeof(float));
    return true;
}

bool read_i32_le(std::ifstream& in, std::int32_t* out) {
    std::uint32_t u = 0;
    if (!read_u32_le(in, &u)) return false;
    *out = static_cast<std::int32_t>(u);
    return true;
}

bool read_i32_le(std::istream& in, std::int32_t* out) {
    std::uint32_t u = 0;
    if (!read_u32_le(in, &u)) return false;
    *out = static_cast<std::int32_t>(u);
    return true;
}

bool read_string_u16(std::ifstream& in, std::string* out) {
    std::uint16_t len = 0;
    if (!read_u16_le(in, &len)) return false;
    out->clear();
    if (len == 0) return true;
    std::vector<char> tmp;
    tmp.resize(static_cast<std::size_t>(len));
    if (!read_bytes(in, tmp.data(), tmp.size())) return false;
    out->assign(tmp.data(), tmp.size());
    return true;
}

bool read_string_u16(std::istream& in, std::string* out) {
    std::uint16_t len = 0;
    if (!read_u16_le(in, &len)) return false;
    out->clear();
    if (len == 0) return true;
    std::vector<char> tmp;
    tmp.resize(static_cast<std::size_t>(len));
    if (!read_bytes(in, tmp.data(), tmp.size())) return false;
    out->assign(tmp.data(), tmp.size());
    return true;
}

bool write_bytes(std::ofstream& out, const void* data, std::size_t size) {
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

bool write_u8(std::ofstream& out, std::uint8_t v) {
    return write_bytes(out, &v, sizeof(v));
}

bool write_u16_le(std::ofstream& out, std::uint16_t v) {
    const unsigned char b[2] = {
        static_cast<unsigned char>(v & 0xffu),
        static_cast<unsigned char>((v >> 8) & 0xffu),
    };
    return write_bytes(out, b, sizeof(b));
}

bool write_u32_le(std::ofstream& out, std::uint32_t v) {
    const unsigned char b[4] = {
        static_cast<unsigned char>(v & 0xffu),
        static_cast<unsigned char>((v >> 8) & 0xffu),
        static_cast<unsigned char>((v >> 16) & 0xffu),
        static_cast<unsigned char>((v >> 24) & 0xffu),
    };
    return write_bytes(out, b, sizeof(b));
}

bool write_f32_le(std::ofstream& out, float v) {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t u = 0;
    std::memcpy(&u, &v, sizeof(float));
    return write_u32_le(out, u);
}

bool write_i32_le(std::ofstream& out, std::int32_t v) {
    return write_u32_le(out, static_cast<std::uint32_t>(v));
}

bool write_string_u16(std::ofstream& out, const std::string& s) {
    if (s.size() > std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }
    if (!write_u16_le(out, static_cast<std::uint16_t>(s.size()))) {
        return false;
    }
    if (!s.empty()) {
        return write_bytes(out, s.data(), s.size());
    }
    return true;
}

static bool is_valid_block_type(std::uint8_t raw) {
    return raw < static_cast<std::uint8_t>(::shared::voxel::BlockType::Count);
}

static std::size_t chunk_index(std::uint8_t lx, std::uint16_t ly, std::uint8_t lz) {
    return static_cast<std::size_t>(ly) * static_cast<std::size_t>(::shared::voxel::CHUNK_WIDTH) * static_cast<std::size_t>(::shared::voxel::CHUNK_DEPTH) +
           static_cast<std::size_t>(lz) * static_cast<std::size_t>(::shared::voxel::CHUNK_WIDTH) +
           static_cast<std::size_t>(lx);
}

} // namespace

bool write_rfmap(const std::filesystem::path& path,
                const ExportRequest& req,
                const BlockGetter& getBlock,
                std::string* outError) {
    if (req.mapId.empty()) {
        if (outError) *outError = "mapId is empty";
        return false;
    }
    if (req.version == 0) {
        if (outError) *outError = "version must be > 0";
        return false;
    }
    if (!getBlock) {
        if (outError) *outError = "getBlock callback is null";
        return false;
    }

    const auto& b = req.bounds;
    if (b.chunkMinX > b.chunkMaxX || b.chunkMinZ > b.chunkMaxZ) {
        if (outError) *outError = "invalid chunk bounds";
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (outError) *outError = "failed to open output file";
        return false;
    }

    // Header
    if (!write_bytes(out, kMagic.data(), kMagic.size())) {
        if (outError) *outError = "failed to write magic";
        return false;
    }
    if (!write_u32_le(out, kFormatVersion)) {
        if (outError) *outError = "failed to write formatVersion";
        return false;
    }

    // Metadata
    if (!write_string_u16(out, req.mapId)) {
        if (outError) *outError = "failed to write mapId";
        return false;
    }
    if (!write_u32_le(out, req.version)) {
        if (outError) *outError = "failed to write version";
        return false;
    }

    // Bounds in chunks
    if (!write_i32_le(out, b.chunkMinX) || !write_i32_le(out, b.chunkMinZ) || !write_i32_le(out, b.chunkMaxX) || !write_i32_le(out, b.chunkMaxZ)) {
        if (outError) *outError = "failed to write chunk bounds";
        return false;
    }

    // World boundary: MT-1 uses chunk bounds as boundary for now.
    if (!write_i32_le(out, b.chunkMinX) || !write_i32_le(out, b.chunkMinZ) || !write_i32_le(out, b.chunkMaxX) || !write_i32_le(out, b.chunkMaxZ)) {
        if (outError) *outError = "failed to write world boundary";
        return false;
    }

    // Placeholder for chunkCount (we'll patch it once we know).
    const auto chunkCountPos = out.tellp();
    if (!write_u32_le(out, 0)) {
        if (outError) *outError = "failed to write chunkCount placeholder";
        return false;
    }

    std::uint32_t chunkCount = 0;

    for (std::int32_t cz = b.chunkMinZ; cz <= b.chunkMaxZ; cz++) {
        for (std::int32_t cx = b.chunkMinX; cx <= b.chunkMaxX; cx++) {
            std::uint32_t blockCount = 0;

            // First pass: count non-air blocks.
            for (std::uint16_t ly = 0; ly < static_cast<std::uint16_t>(shared::voxel::CHUNK_HEIGHT); ly++) {
                for (std::uint8_t lz = 0; lz < static_cast<std::uint8_t>(shared::voxel::CHUNK_DEPTH); lz++) {
                    for (std::uint8_t lx = 0; lx < static_cast<std::uint8_t>(shared::voxel::CHUNK_WIDTH); lx++) {
                        const int wx = static_cast<int>(cx) * shared::voxel::CHUNK_WIDTH + static_cast<int>(lx);
                        const int wy = static_cast<int>(ly);
                        const int wz = static_cast<int>(cz) * shared::voxel::CHUNK_DEPTH + static_cast<int>(lz);
                        const auto bt = getBlock(wx, wy, wz);
                        if (bt != shared::voxel::BlockType::Air) {
                            blockCount++;
                        }
                    }
                }
            }

            if (blockCount == 0) {
                continue;
            }

            // Chunk header
            if (!write_i32_le(out, cx) || !write_i32_le(out, cz) || !write_u32_le(out, blockCount)) {
                if (outError) *outError = "failed to write chunk header";
                return false;
            }

            // Second pass: write blocks.
            for (std::uint16_t ly = 0; ly < static_cast<std::uint16_t>(shared::voxel::CHUNK_HEIGHT); ly++) {
                for (std::uint8_t lz = 0; lz < static_cast<std::uint8_t>(shared::voxel::CHUNK_DEPTH); lz++) {
                    for (std::uint8_t lx = 0; lx < static_cast<std::uint8_t>(shared::voxel::CHUNK_WIDTH); lx++) {
                        const int wx = static_cast<int>(cx) * shared::voxel::CHUNK_WIDTH + static_cast<int>(lx);
                        const int wy = static_cast<int>(ly);
                        const int wz = static_cast<int>(cz) * shared::voxel::CHUNK_DEPTH + static_cast<int>(lz);
                        const auto bt = getBlock(wx, wy, wz);
                        if (bt == shared::voxel::BlockType::Air) {
                            continue;
                        }

                        if (!write_u8(out, lx) || !write_u16_le(out, ly) || !write_u8(out, lz) || !write_u8(out, static_cast<std::uint8_t>(bt))) {
                            if (outError) *outError = "failed to write block record";
                            return false;
                        }
                    }
                }
            }

            chunkCount++;
        }
    }

    // Patch chunkCount.
    const auto endPos = out.tellp();
    out.seekp(chunkCountPos);
    if (!write_u32_le(out, chunkCount)) {
        if (outError) *outError = "failed to patch chunkCount";
        return false;
    }
    out.seekp(endPos);

    // MT-1/MV-1 forward-compat: section table.
    // Format v2+: u32 sectionCount, then [tag:u32][size:u32][payload...].
    // MV-1 requires VisualSettings section.
    // MT-1 adds an optional protection allow-list section.
    if (!write_u32_le(out, 2)) {
        if (outError) *outError = "failed to write sectionCount";
        return false;
    }

    // MV-1: VisualSettings
    if (!write_u32_le(out, kSectionTagVisualSettings) || !write_u32_le(out, kVisualSettingsPayloadSize)) {
        if (outError) *outError = "failed to write VisualSettings section header";
        return false;
    }

    const auto vs = req.visualSettings;
    const std::uint8_t skybox = static_cast<std::uint8_t>(vs.skyboxKind);
    const std::uint8_t useMoon = vs.useMoon ? 1u : 0u;
    const std::uint16_t reserved = 0;
    if (!write_u8(out, skybox) || !write_u8(out, useMoon) || !write_u16_le(out, reserved) ||
        !write_f32_le(out, vs.timeOfDayHours) ||
        !write_f32_le(out, vs.sunIntensity) ||
        !write_f32_le(out, vs.ambientIntensity) ||
        !write_f32_le(out, vs.temperature) ||
        !write_f32_le(out, vs.humidity)) {
        if (outError) *outError = "failed to write VisualSettings payload";
        return false;
    }

    // MT-1: Protection allow-list (by BlockType id)
    if (!write_u32_le(out, kSectionTagProtection) || !write_u32_le(out, kProtectionPayloadSize)) {
        if (outError) *outError = "failed to write Protection section header";
        return false;
    }

    for (std::size_t i = 0; i < req.breakableTemplateBlocks.size(); i++) {
        const std::uint8_t v = req.breakableTemplateBlocks[i] ? 1u : 0u;
        if (!write_u8(out, v)) {
            if (outError) *outError = "failed to write Protection payload";
            return false;
        }
    }

    if (!out) {
        if (outError) *outError = "failed while writing output";
        return false;
    }

    return true;
}

bool read_rfmap(const std::filesystem::path& path,
               MapTemplate* outMap,
               std::string* outError) {
    if (!outMap) {
        if (outError) *outError = "outMap is null";
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (outError) *outError = "failed to open input file";
        return false;
    }

    std::array<unsigned char, 4> magic{};
    if (!read_bytes(in, magic.data(), magic.size())) {
        if (outError) *outError = "failed to read magic";
        return false;
    }
    if (magic != kMagic) {
        if (outError) *outError = "bad magic";
        return false;
    }

    std::uint32_t formatVersion = 0;
    if (!read_u32_le(in, &formatVersion)) {
        if (outError) *outError = "failed to read formatVersion";
        return false;
    }
    if (formatVersion == 0 || formatVersion > kFormatVersion) {
        if (outError) *outError = "unsupported formatVersion";
        return false;
    }

    MapTemplate map;
    map.visualSettings = default_visual_settings();

    if (!read_string_u16(in, &map.mapId)) {
        if (outError) *outError = "failed to read mapId";
        return false;
    }
    if (map.mapId.empty()) {
        if (outError) *outError = "mapId is empty";
        return false;
    }

    if (!read_u32_le(in, &map.version)) {
        if (outError) *outError = "failed to read version";
        return false;
    }
    if (map.version == 0) {
        if (outError) *outError = "version must be > 0";
        return false;
    }

    if (!read_i32_le(in, &map.bounds.chunkMinX) || !read_i32_le(in, &map.bounds.chunkMinZ) ||
        !read_i32_le(in, &map.bounds.chunkMaxX) || !read_i32_le(in, &map.bounds.chunkMaxZ)) {
        if (outError) *outError = "failed to read chunk bounds";
        return false;
    }
    if (map.bounds.chunkMinX > map.bounds.chunkMaxX || map.bounds.chunkMinZ > map.bounds.chunkMaxZ) {
        if (outError) *outError = "invalid chunk bounds";
        return false;
    }

    // World boundary (MT-1). v1 files store it as chunk bounds.
    if (!read_i32_le(in, &map.worldBoundary.chunkMinX) || !read_i32_le(in, &map.worldBoundary.chunkMinZ) ||
        !read_i32_le(in, &map.worldBoundary.chunkMaxX) || !read_i32_le(in, &map.worldBoundary.chunkMaxZ)) {
        if (outError) *outError = "failed to read world boundary";
        return false;
    }
    if (map.worldBoundary.chunkMinX > map.worldBoundary.chunkMaxX || map.worldBoundary.chunkMinZ > map.worldBoundary.chunkMaxZ) {
        if (outError) *outError = "invalid world boundary";
        return false;
    }

    std::uint32_t chunkCount = 0;
    if (!read_u32_le(in, &chunkCount)) {
        if (outError) *outError = "failed to read chunkCount";
        return false;
    }

    // Pre-reserve to avoid rehash churn.
    map.chunks.reserve(chunkCount);

    for (std::uint32_t ci = 0; ci < chunkCount; ci++) {
        std::int32_t cx = 0;
        std::int32_t cz = 0;
        std::uint32_t blockCount = 0;
        if (!read_i32_le(in, &cx) || !read_i32_le(in, &cz) || !read_u32_le(in, &blockCount)) {
            if (outError) *outError = "failed to read chunk header";
            return false;
        }

        MapTemplate::ChunkData chunk;
        chunk.blocks.fill(shared::voxel::BlockType::Air);

        for (std::uint32_t bi = 0; bi < blockCount; bi++) {
            std::uint8_t lx = 0;
            std::uint16_t ly = 0;
            std::uint8_t lz = 0;
            std::uint8_t rawType = 0;
            if (!read_u8(in, &lx) || !read_u16_le(in, &ly) || !read_u8(in, &lz) || !read_u8(in, &rawType)) {
                if (outError) *outError = "failed to read block record";
                return false;
            }

            if (lx >= static_cast<std::uint8_t>(shared::voxel::CHUNK_WIDTH) ||
                lz >= static_cast<std::uint8_t>(shared::voxel::CHUNK_DEPTH) ||
                ly >= static_cast<std::uint16_t>(shared::voxel::CHUNK_HEIGHT)) {
                if (outError) *outError = "block record out of range";
                return false;
            }
            if (!is_valid_block_type(rawType)) {
                if (outError) *outError = "invalid blockType id";
                return false;
            }

            const auto bt = static_cast<shared::voxel::BlockType>(rawType);
            if (bt == shared::voxel::BlockType::Air) {
                // Sparse encoding should not store air.
                continue;
            }

            chunk.blocks[chunk_index(lx, ly, lz)] = bt;
        }

        map.chunks[{cx, cz}] = std::move(chunk);
    }

    // v2+: optional section table.
    if (formatVersion >= 2) {
        // If file ends exactly after chunks, treat as 0 sections.
        std::uint32_t sectionCount = 0;
        const auto posBefore = in.tellg();
        if (read_u32_le(in, &sectionCount)) {
            for (std::uint32_t si = 0; si < sectionCount; si++) {
                std::uint32_t tag = 0;
                std::uint32_t size = 0;
                if (!read_u32_le(in, &tag) || !read_u32_le(in, &size)) {
                    if (outError) *outError = "failed to read section header";
                    return false;
                }

                if (tag == kSectionTagVisualSettings) {
                    // MV-1 fixed-size payload; tolerate larger payloads by reading known prefix and skipping the rest.
                    if (size < kVisualSettingsPayloadMinSize) {
                        if (outError) *outError = "VisualSettings section too small";
                        return false;
                    }

                    std::uint8_t skybox = 0;
                    std::uint8_t useMoon = 0;
                    std::uint16_t reserved = 0;
                    float timeOfDay = 12.0f;
                    float sunI = 1.0f;
                    float ambI = 0.25f;

                    // MV-2 optional
                    float temp = map.visualSettings.temperature;
                    // MV-3 optional
                    float hum = map.visualSettings.humidity;

                    if (!read_u8(in, &skybox) || !read_u8(in, &useMoon) || !read_u16_le(in, &reserved) ||
                        !read_f32_le(in, &timeOfDay) || !read_f32_le(in, &sunI) || !read_f32_le(in, &ambI)) {
                        if (outError) *outError = "failed to read VisualSettings payload";
                        return false;
                    }

                    if (size >= kVisualSettingsPayloadSizeV2) {
                        if (!read_f32_le(in, &temp)) {
                            if (outError) *outError = "failed to read VisualSettings temperature";
                            return false;
                        }
                    }

                    if (size >= kVisualSettingsPayloadSize) {
                        if (!read_f32_le(in, &hum)) {
                            if (outError) *outError = "failed to read VisualSettings humidity";
                            return false;
                        }
                    }

                    map.visualSettings.skyboxKind = static_cast<MapTemplate::SkyboxKind>(skybox);
                    map.visualSettings.useMoon = (useMoon != 0);
                    map.visualSettings.timeOfDayHours = timeOfDay;
                    map.visualSettings.sunIntensity = sunI;
                    map.visualSettings.ambientIntensity = ambI;
                    map.visualSettings.temperature = temp;
                    map.visualSettings.humidity = hum;

                    const std::uint32_t consumed = (size >= kVisualSettingsPayloadSize) ? kVisualSettingsPayloadSize :
                                                   (size >= kVisualSettingsPayloadSizeV2) ? kVisualSettingsPayloadSizeV2 :
                                                   kVisualSettingsPayloadMinSize;
                    const std::uint32_t remaining = size - consumed;
                    if (remaining > 0) {
                        in.seekg(static_cast<std::streamoff>(remaining), std::ios::cur);
                        if (!in) {
                            if (outError) *outError = "failed to skip VisualSettings padding";
                            return false;
                        }
                    }
                } else {
                    if (tag == kSectionTagProtection) {
                        // MT-1 fixed-size payload; tolerate larger payloads by reading known prefix and skipping the rest.
                        if (size < kProtectionPayloadSize) {
                            if (outError) *outError = "Protection section too small";
                            return false;
                        }

                        for (std::size_t i = 0; i < map.breakableTemplateBlocks.size(); i++) {
                            std::uint8_t v = 0;
                            if (!read_u8(in, &v)) {
                                if (outError) *outError = "failed to read Protection payload";
                                return false;
                            }
                            map.breakableTemplateBlocks[i] = (v != 0);
                        }

                        const std::uint32_t remaining = size - kProtectionPayloadSize;
                        if (remaining > 0) {
                            in.seekg(static_cast<std::streamoff>(remaining), std::ios::cur);
                            if (!in) {
                                if (outError) *outError = "failed to skip Protection padding";
                                return false;
                            }
                        }
                    } else {
                        // Skip unknown sections for forward compatibility.
                        if (size > 0) {
                            in.seekg(static_cast<std::streamoff>(size), std::ios::cur);
                            if (!in) {
                                if (outError) *outError = "failed to skip section";
                                return false;
                            }
                        }
                    }
                }
            }
        } else {
            in.clear();
            in.seekg(posBefore);
        }
    }

    *outMap = std::move(map);
    return true;
}

bool read_rfmap_from_memory(const void* data,
                            std::size_t size,
                            MapTemplate* outMap,
                            std::string* outError) {
    if (!outMap) {
        if (outError) *outError = "outMap is null";
        return false;
    }
    if (!data || size == 0) {
        if (outError) *outError = "empty data buffer";
        return false;
    }

    // Create istringstream from memory buffer.
    std::string buf(reinterpret_cast<const char*>(data), size);
    std::istringstream in(buf, std::ios::binary);

    std::array<unsigned char, 4> magic{};
    if (!read_bytes(in, magic.data(), magic.size())) {
        if (outError) *outError = "failed to read magic";
        return false;
    }
    if (magic != kMagic) {
        if (outError) *outError = "bad magic";
        return false;
    }

    std::uint32_t formatVersion = 0;
    if (!read_u32_le(in, &formatVersion)) {
        if (outError) *outError = "failed to read formatVersion";
        return false;
    }
    if (formatVersion == 0 || formatVersion > kFormatVersion) {
        if (outError) *outError = "unsupported formatVersion";
        return false;
    }

    MapTemplate map;
    map.visualSettings = default_visual_settings();

    if (!read_string_u16(in, &map.mapId)) {
        if (outError) *outError = "failed to read mapId";
        return false;
    }
    if (map.mapId.empty()) {
        if (outError) *outError = "mapId is empty";
        return false;
    }

    if (!read_u32_le(in, &map.version)) {
        if (outError) *outError = "failed to read version";
        return false;
    }
    if (map.version == 0) {
        if (outError) *outError = "version must be > 0";
        return false;
    }

    if (!read_i32_le(in, &map.bounds.chunkMinX) || !read_i32_le(in, &map.bounds.chunkMinZ) ||
        !read_i32_le(in, &map.bounds.chunkMaxX) || !read_i32_le(in, &map.bounds.chunkMaxZ)) {
        if (outError) *outError = "failed to read chunk bounds";
        return false;
    }
    if (map.bounds.chunkMinX > map.bounds.chunkMaxX || map.bounds.chunkMinZ > map.bounds.chunkMaxZ) {
        if (outError) *outError = "invalid chunk bounds";
        return false;
    }

    if (!read_i32_le(in, &map.worldBoundary.chunkMinX) || !read_i32_le(in, &map.worldBoundary.chunkMinZ) ||
        !read_i32_le(in, &map.worldBoundary.chunkMaxX) || !read_i32_le(in, &map.worldBoundary.chunkMaxZ)) {
        if (outError) *outError = "failed to read world boundary";
        return false;
    }
    if (map.worldBoundary.chunkMinX > map.worldBoundary.chunkMaxX || map.worldBoundary.chunkMinZ > map.worldBoundary.chunkMaxZ) {
        if (outError) *outError = "invalid world boundary";
        return false;
    }

    std::uint32_t chunkCount = 0;
    if (!read_u32_le(in, &chunkCount)) {
        if (outError) *outError = "failed to read chunkCount";
        return false;
    }

    map.chunks.reserve(chunkCount);

    for (std::uint32_t ci = 0; ci < chunkCount; ci++) {
        std::int32_t cx = 0;
        std::int32_t cz = 0;
        std::uint32_t blockCount = 0;
        if (!read_i32_le(in, &cx) || !read_i32_le(in, &cz) || !read_u32_le(in, &blockCount)) {
            if (outError) *outError = "failed to read chunk header";
            return false;
        }

        MapTemplate::ChunkData chunk;
        chunk.blocks.fill(shared::voxel::BlockType::Air);

        for (std::uint32_t bi = 0; bi < blockCount; bi++) {
            std::uint8_t lx = 0;
            std::uint16_t ly = 0;
            std::uint8_t lz = 0;
            std::uint8_t rawType = 0;
            if (!read_u8(in, &lx) || !read_u16_le(in, &ly) || !read_u8(in, &lz) || !read_u8(in, &rawType)) {
                if (outError) *outError = "failed to read block record";
                return false;
            }

            if (lx >= static_cast<std::uint8_t>(shared::voxel::CHUNK_WIDTH) ||
                lz >= static_cast<std::uint8_t>(shared::voxel::CHUNK_DEPTH) ||
                ly >= static_cast<std::uint16_t>(shared::voxel::CHUNK_HEIGHT)) {
                if (outError) *outError = "block record out of range";
                return false;
            }
            if (!is_valid_block_type(rawType)) {
                if (outError) *outError = "invalid blockType id";
                return false;
            }

            const auto bt = static_cast<shared::voxel::BlockType>(rawType);
            if (bt == shared::voxel::BlockType::Air) {
                continue;
            }

            chunk.blocks[chunk_index(lx, ly, lz)] = bt;
        }

        map.chunks[{cx, cz}] = std::move(chunk);
    }

    // v2+: optional section table.
    if (formatVersion >= 2) {
        std::uint32_t sectionCount = 0;
        const auto posBefore = in.tellg();
        if (read_u32_le(in, &sectionCount)) {
            for (std::uint32_t si = 0; si < sectionCount; si++) {
                std::uint32_t tag = 0;
                std::uint32_t sectionSize = 0;
                if (!read_u32_le(in, &tag) || !read_u32_le(in, &sectionSize)) {
                    if (outError) *outError = "failed to read section header";
                    return false;
                }

                if (tag == kSectionTagVisualSettings) {
                    if (sectionSize < kVisualSettingsPayloadMinSize) {
                        if (outError) *outError = "VisualSettings section too small";
                        return false;
                    }

                    std::uint8_t skybox = 0;
                    std::uint8_t useMoon = 0;
                    std::uint16_t reserved = 0;
                    float timeOfDay = 12.0f;
                    float sunI = 1.0f;
                    float ambI = 0.25f;
                    float temp = map.visualSettings.temperature;
                    float hum = map.visualSettings.humidity;

                    if (!read_u8(in, &skybox) || !read_u8(in, &useMoon) || !read_u16_le(in, &reserved) ||
                        !read_f32_le(in, &timeOfDay) || !read_f32_le(in, &sunI) || !read_f32_le(in, &ambI)) {
                        if (outError) *outError = "failed to read VisualSettings payload";
                        return false;
                    }

                    if (sectionSize >= kVisualSettingsPayloadSizeV2) {
                        if (!read_f32_le(in, &temp)) {
                            if (outError) *outError = "failed to read VisualSettings temperature";
                            return false;
                        }
                    }
                    if (sectionSize >= kVisualSettingsPayloadSize) {
                        if (!read_f32_le(in, &hum)) {
                            if (outError) *outError = "failed to read VisualSettings humidity";
                            return false;
                        }
                    }

                    map.visualSettings.skyboxKind = static_cast<MapTemplate::SkyboxKind>(skybox);
                    map.visualSettings.useMoon = (useMoon != 0);
                    map.visualSettings.timeOfDayHours = timeOfDay;
                    map.visualSettings.sunIntensity = sunI;
                    map.visualSettings.ambientIntensity = ambI;
                    map.visualSettings.temperature = temp;
                    map.visualSettings.humidity = hum;

                    const std::uint32_t consumed = (sectionSize >= kVisualSettingsPayloadSize) ? kVisualSettingsPayloadSize :
                                                   (sectionSize >= kVisualSettingsPayloadSizeV2) ? kVisualSettingsPayloadSizeV2 :
                                                   kVisualSettingsPayloadMinSize;
                    const std::uint32_t remaining = sectionSize - consumed;
                    if (remaining > 0) {
                        in.seekg(static_cast<std::streamoff>(remaining), std::ios::cur);
                        if (!in) {
                            if (outError) *outError = "failed to skip VisualSettings padding";
                            return false;
                        }
                    }
                } else if (tag == kSectionTagProtection) {
                    if (sectionSize < kProtectionPayloadSize) {
                        if (outError) *outError = "Protection section too small";
                        return false;
                    }

                    for (std::size_t i = 0; i < map.breakableTemplateBlocks.size(); i++) {
                        std::uint8_t v = 0;
                        if (!read_u8(in, &v)) {
                            if (outError) *outError = "failed to read Protection payload";
                            return false;
                        }
                        map.breakableTemplateBlocks[i] = (v != 0);
                    }

                    const std::uint32_t remaining = sectionSize - kProtectionPayloadSize;
                    if (remaining > 0) {
                        in.seekg(static_cast<std::streamoff>(remaining), std::ios::cur);
                        if (!in) {
                            if (outError) *outError = "failed to skip Protection padding";
                            return false;
                        }
                    }
                } else {
                    if (sectionSize > 0) {
                        in.seekg(static_cast<std::streamoff>(sectionSize), std::ios::cur);
                        if (!in) {
                            if (outError) *outError = "failed to skip section";
                            return false;
                        }
                    }
                }
            }
        } else {
            in.clear();
            in.seekg(posBefore);
        }
    }

    *outMap = std::move(map);
    return true;
}

} // namespace shared::maps
