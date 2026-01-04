#pragma once

#include "engine/modules/voxel/shared/block.hpp"
#include "engine/modules/voxel/shared/block_state.hpp"

#include "engine/maps/rfmap_io.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bedwars::voxel {

class Terrain {
public:
    explicit Terrain(std::uint32_t seed);

    std::uint32_t seed() const { return seed_; }

    shared::voxel::BlockType get_block(int x, int y, int z) const;
    void set_block(int x, int y, int z, shared::voxel::BlockType type);

    // Gameplay edits: used by authoritative server validators.
    void place_player_block(int x, int y, int z, shared::voxel::BlockType type);
    void break_player_block(int x, int y, int z);
    bool is_player_placed(int x, int y, int z) const;

    // BedWars rule: in templated matches, only player-placed blocks are breakable by default.
    // Template blocks may be broken only if allow-listed in the template metadata.
    bool can_player_break(int x, int y, int z, shared::voxel::BlockType current) const;

    bool has_map_template() const { return map_template_.has_value(); }
    const shared::maps::MapTemplate* map_template() const { return map_template_ ? &*map_template_ : nullptr; }
    void set_map_template(shared::maps::MapTemplate map);

    // Get all block modifications (for sending to new clients)
    struct BlockModification {
        int x, y, z;
        shared::voxel::BlockType type;
        shared::voxel::BlockRuntimeState state;
    };
    std::vector<BlockModification> get_all_modifications() const;

    // Block state management
    shared::voxel::BlockRuntimeState get_block_state(int x, int y, int z) const;
    void set_block_state(int x, int y, int z, shared::voxel::BlockRuntimeState state);
    
    // Compute connections for a block based on neighbors
    shared::voxel::BlockRuntimeState compute_block_state(int x, int y, int z, shared::voxel::BlockType type) const;
    
    // Update neighbor connections when a block changes
    // Returns list of positions that were updated (for broadcasting)
    std::vector<BlockModification> update_neighbor_states(int x, int y, int z);

    // Get full chunk data for replication (16x256x16 = 65536 blocks)
    // Returns block types in Y-major order: index = y * 256 + z * 16 + x (local coords)
    std::vector<std::uint8_t> get_chunk_data(int chunkX, int chunkZ) const;

    // Editor mode helper: when no map template is set, treat the base world as empty/void (all Air).
    // This ensures map exports contain only authored blocks, not procedural terrain.
    void set_void_base(bool enabled) { void_base_ = enabled; }

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

    void set_override_(int x, int y, int z, shared::voxel::BlockType type, bool keep_if_matches_base);

    void init_perlin_() const;
    float perlin_noise_(float x, float y) const;
    float octave_perlin_(float x, float y, int octaves, float persistence) const;

    std::uint32_t seed_{0};

    bool void_base_{false};

    std::optional<shared::maps::MapTemplate> map_template_{};

    // Sparse runtime modifications (placed/broken blocks) on top of procedural base terrain.
    std::unordered_map<BlockKey, shared::voxel::BlockType, BlockKeyHash> overrides_{};
    
    // Block states for blocks that have non-default state (connections, slab type)
    std::unordered_map<BlockKey, shared::voxel::BlockRuntimeState, BlockKeyHash> block_states_{};

    // Positions whose current block was placed by a player during the match.
    // Only these are breakable by default in a templated match.
    std::unordered_set<BlockKey, BlockKeyHash> player_placed_{};

    mutable std::array<unsigned char, 512> perm_{};
    mutable bool perm_initialized_{false};
};

} // namespace bedwars::voxel
