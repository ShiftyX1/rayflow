#pragma once

#include "block.hpp"
#include "block_shape.hpp"  // For SlabType
#include <cstdint>
#include <array>

namespace shared::voxel {

// SlabType is now defined in block_shape.hpp to avoid circular dependencies

/**
 * @brief Connection group for blocks that can connect to neighbors.
 */
enum class ConnectionGroup : std::uint8_t {
    None = 0,    // No connections (regular blocks)
    Fence,       // Connects to fences, walls, solid blocks
    Wall,        // Connects to walls, fences, solid blocks
    GlassPane,   // Connects to panes, solid blocks
};

/**
 * @brief Runtime block state with connection flags and orientation.
 * 
 * This is a compact representation that can be stored alongside BlockType.
 * Different from BlockState in block_shape.hpp which is for JSON model definitions.
 * Total size: 2 bytes (fits in 16-bit with BlockType).
 */
struct BlockRuntimeState {
    // Connection flags for fences/walls/panes (4 bits)
    bool north : 1;
    bool south : 1;
    bool east  : 1;
    bool west  : 1;
    
    // Slab type (2 bits)
    SlabType slabType : 2;
    
    // Reserved for future use (stairs orientation, etc.)
    std::uint8_t reserved : 2;
    
    // Default constructor
    constexpr BlockRuntimeState() 
        : north(false), south(false), east(false), west(false)
        , slabType(SlabType::Bottom), reserved(0) {}
    
    // Default state (no connections, bottom slab)
    static constexpr BlockRuntimeState defaults() {
        return BlockRuntimeState{};
    }
    
    // Full connections (all 4 directions)
    static constexpr BlockRuntimeState all_connected() {
        BlockRuntimeState s{};
        s.north = s.south = s.east = s.west = true;
        return s;
    }
    
    // Check if any connection is active
    constexpr bool has_connections() const {
        return north || south || east || west;
    }
    
    // Count active connections
    constexpr int connection_count() const {
        return (north ? 1 : 0) + (south ? 1 : 0) + (east ? 1 : 0) + (west ? 1 : 0);
    }
    
    // Equality
    constexpr bool operator==(const BlockRuntimeState& other) const {
        return north == other.north && south == other.south &&
               east == other.east && west == other.west &&
               slabType == other.slabType;
    }
    
    constexpr bool operator!=(const BlockRuntimeState& other) const {
        return !(*this == other);
    }
    
    // Serialize to single byte for network/storage
    constexpr std::uint8_t to_byte() const {
        return static_cast<std::uint8_t>(
            (north ? 0x01 : 0) |
            (south ? 0x02 : 0) |
            (east  ? 0x04 : 0) |
            (west  ? 0x08 : 0) |
            (static_cast<std::uint8_t>(slabType) << 4)
        );
    }
    
    // Deserialize from byte
    static constexpr BlockRuntimeState from_byte(std::uint8_t b) {
        BlockRuntimeState s{};
        s.north = (b & 0x01) != 0;
        s.south = (b & 0x02) != 0;
        s.east  = (b & 0x04) != 0;
        s.west  = (b & 0x08) != 0;
        s.slabType = static_cast<SlabType>((b >> 4) & 0x03);
        return s;
    }
};

static_assert(sizeof(BlockRuntimeState) <= 2, "BlockRuntimeState must fit in 2 bytes");

// ============================================================================
// Block Category Functions
// ============================================================================

/**
 * @brief Get the connection group for a block type.
 */
constexpr ConnectionGroup get_connection_group(BlockType type) {
    switch (type) {
        case BlockType::OakFence:
            return ConnectionGroup::Fence;
        // Future: add walls, glass panes, etc.
        default:
            return ConnectionGroup::None;
    }
}

/**
 * @brief Check if a block type is a fence.
 */
constexpr bool is_fence(BlockType type) {
    return get_connection_group(type) == ConnectionGroup::Fence;
}

/**
 * @brief Check if a block type is a wall.
 */
constexpr bool is_wall(BlockType type) {
    return get_connection_group(type) == ConnectionGroup::Wall;
}

/**
 * @brief Check if block type uses connections (fence/wall/pane).
 */
constexpr bool uses_connections(BlockType type) {
    return get_connection_group(type) != ConnectionGroup::None;
}

// ============================================================================
// Slab Functions
// ============================================================================

/**
 * @brief Get the slab category for a block type.
 * Slabs of the same category can merge into double slabs.
 */
enum class SlabCategory : std::uint8_t {
    NotSlab = 0,
    Stone,
    Wood,
    // Add more as needed
};

constexpr SlabCategory get_slab_category(BlockType type) {
    switch (type) {
        case BlockType::StoneSlab:
        case BlockType::StoneSlabTop:
            return SlabCategory::Stone;
        case BlockType::WoodSlab:
        case BlockType::WoodSlabTop:
            return SlabCategory::Wood;
        default:
            return SlabCategory::NotSlab;
    }
}

/**
 * @brief Get the default slab type for a block type (bottom or top).
 * Default is Bottom; actual placement determined by hitY from client.
 */
constexpr SlabType get_default_slab_type(BlockType /*type*/) {
    // All slabs default to bottom; actual type determined at placement time
    return SlabType::Bottom;
}

/**
 * @brief Determine slab placement type from click position.
 * @param hitY Local Y position within clicked block (0-1)
 * @param clickedFace Face that was clicked (2=+Y top, 3=-Y bottom)
 * @return SlabType::Top if placing on upper half, Bottom otherwise
 */
constexpr SlabType determine_slab_type_from_hit(float hitY, std::uint8_t clickedFace) {
    // Clicking bottom of a block (-Y face) = place top slab
    if (clickedFace == 3) return SlabType::Top;
    // Clicking top of a block (+Y face) = place bottom slab  
    if (clickedFace == 2) return SlabType::Bottom;
    // Clicking side faces: depends on hitY within clicked block
    return (hitY > 0.5f) ? SlabType::Top : SlabType::Bottom;
}

/**
 * @brief Get the double (full) block type for a slab category.
 * Returns Air if no double type exists.
 */
constexpr BlockType get_double_slab_type(SlabCategory cat) {
    switch (cat) {
        case SlabCategory::Stone:
            return BlockType::Stone;
        case SlabCategory::Wood:
            return BlockType::Wood;
        default:
            return BlockType::Air;
    }
}

/**
 * @brief Check if two slab types can merge into a double slab.
 * Bottom + Top of same category = Double.
 */
constexpr bool can_slabs_merge(BlockType existing, SlabType existingState,
                                BlockType placing, SlabType placingState) {
    // Must be same slab category
    SlabCategory cat1 = get_slab_category(existing);
    SlabCategory cat2 = get_slab_category(placing);
    
    if (cat1 == SlabCategory::NotSlab || cat1 != cat2) {
        return false;
    }
    
    // One must be bottom, other must be top
    return (existingState == SlabType::Bottom && placingState == SlabType::Top) ||
           (existingState == SlabType::Top && placingState == SlabType::Bottom);
}

// ============================================================================
// Connection Logic
// ============================================================================

/**
 * @brief Check if a block type can be connected to by a fence/wall.
 */
constexpr bool can_fence_connect_to(BlockType type) {
    // Fences connect to:
    // 1. Other fences
    if (is_fence(type)) return true;
    
    // 2. Walls
    if (is_wall(type)) return true;
    
    // 3. Solid full blocks (not slabs, not air, not water)
    if (type == BlockType::Air || type == BlockType::Water || type == BlockType::Light) {
        return false;
    }
    
    // Slabs are not full blocks
    if (is_slab(type)) return false;
    
    // Everything else is solid
    return true;
}

/**
 * @brief Direction offsets for neighbor checking.
 * Index: 0=North(-Z), 1=South(+Z), 2=East(+X), 3=West(-X)
 */
struct Direction {
    int dx, dy, dz;
    
    static constexpr Direction North() { return {0, 0, -1}; }
    static constexpr Direction South() { return {0, 0, +1}; }
    static constexpr Direction East()  { return {+1, 0, 0}; }
    static constexpr Direction West()  { return {-1, 0, 0}; }
    
    static constexpr std::array<Direction, 4> horizontal() {
        return {North(), South(), East(), West()};
    }
};

// ============================================================================
// Collision with State
// ============================================================================

/**
 * @brief Get collision boxes for a fence based on its connection state.
 * Returns up to 5 boxes (post + 4 arms).
 */
struct FenceCollision {
    // Central post (always present)
    static constexpr float POST_MIN_XZ = 6.0f / 16.0f;
    static constexpr float POST_MAX_XZ = 10.0f / 16.0f;
    static constexpr float POST_HEIGHT = 1.5f;  // 24/16, prevents jumping
    
    // Arm dimensions
    static constexpr float ARM_WIDTH = 2.0f / 16.0f;  // 2 pixels wide
    
    BlockCollisionInfo post;
    BlockCollisionInfo northArm;
    BlockCollisionInfo southArm;
    BlockCollisionInfo eastArm;
    BlockCollisionInfo westArm;
    
    static constexpr FenceCollision create() {
        FenceCollision fc{};
        
        // Post: 6-10 XZ, 0-1.5 Y
        fc.post = {POST_MIN_XZ, POST_MAX_XZ, 0.0f, POST_HEIGHT, POST_MIN_XZ, POST_MAX_XZ, true};
        
        // North arm: extends from post to Z=0
        fc.northArm = {POST_MIN_XZ, POST_MAX_XZ, 0.0f, POST_HEIGHT, 0.0f, POST_MIN_XZ, true};
        
        // South arm: extends from post to Z=1
        fc.southArm = {POST_MIN_XZ, POST_MAX_XZ, 0.0f, POST_HEIGHT, POST_MAX_XZ, 1.0f, true};
        
        // East arm: extends from post to X=1
        fc.eastArm = {POST_MAX_XZ, 1.0f, 0.0f, POST_HEIGHT, POST_MIN_XZ, POST_MAX_XZ, true};
        
        // West arm: extends from post to X=0
        fc.westArm = {0.0f, POST_MIN_XZ, 0.0f, POST_HEIGHT, POST_MIN_XZ, POST_MAX_XZ, true};
        
        return fc;
    }
};

/**
 * @brief Get collision info for a block with state.
 * This extends get_collision_info() to account for connections and slab states.
 * 
 * @param type The block type
 * @param state The block state (connections, slab type)
 * @param outBoxes Output array for collision boxes (max 5 for fences)
 * @return Number of collision boxes written
 */
inline int get_collision_boxes(BlockType type, BlockRuntimeState state,
                               BlockCollisionInfo* outBoxes, int maxBoxes) {
    if (maxBoxes <= 0) return 0;
    
    // Fences: variable collision based on connections
    if (is_fence(type)) {
        constexpr auto fc = FenceCollision::create();
        int count = 0;
        
        // Always add post
        outBoxes[count++] = fc.post;
        
        // Add arms based on connections
        if (state.north && count < maxBoxes) outBoxes[count++] = fc.northArm;
        if (state.south && count < maxBoxes) outBoxes[count++] = fc.southArm;
        if (state.east && count < maxBoxes) outBoxes[count++] = fc.eastArm;
        if (state.west && count < maxBoxes) outBoxes[count++] = fc.westArm;
        
        return count;
    }
    
    // Slabs: collision depends on slab type
    if (is_slab(type)) {
        if (state.slabType == SlabType::Double) {
            // Double slab = full block
            outBoxes[0] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, true};
        } else if (state.slabType == SlabType::Top) {
            outBoxes[0] = {0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 1.0f, true};
        } else {
            outBoxes[0] = {0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 1.0f, true};
        }
        return 1;
    }
    
    // Default: use standard collision info
    outBoxes[0] = get_collision_info(type);
    return outBoxes[0].hasCollision ? 1 : 0;
}

/**
 * @brief Get the primary (largest) collision box for simple checks.
 * For fences, returns the post. For slabs, returns based on state.
 */
inline BlockCollisionInfo get_primary_collision(BlockType type, BlockRuntimeState state) {
    BlockCollisionInfo boxes[5];
    int count = get_collision_boxes(type, state, boxes, 5);
    return count > 0 ? boxes[0] : BlockCollisionInfo{0, 1, 0, 0, 0, 1, false};
}

} // namespace shared::voxel
