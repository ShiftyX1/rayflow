#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>

namespace shared::voxel {

/**
 * @brief Slab placement type within a block position.
 * Defined here to avoid circular dependency with block_state.hpp.
 */
enum class SlabType : std::uint8_t {
    Bottom = 0,  // Occupies lower half (Y: 0-0.5)
    Top,         // Occupies upper half (Y: 0.5-1)
    Double       // Full block (merged slabs)
};

/**
 * @brief Axis-aligned bounding box for collision and rendering.
 * Coordinates are in block-local space [0,1] range, where (0,0,0) is the block origin.
 */
struct AABB {
    float minX{0.0f}, minY{0.0f}, minZ{0.0f};
    float maxX{1.0f}, maxY{1.0f}, maxZ{1.0f};
    
    constexpr AABB() = default;
    constexpr AABB(float x0, float y0, float z0, float x1, float y1, float z1)
        : minX(x0), minY(y0), minZ(z0), maxX(x1), maxY(y1), maxZ(z1) {}
    
    // Full block AABB
    static constexpr AABB full() { return AABB{0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}; }
    
    // Bottom slab (half height at bottom)
    static constexpr AABB bottom_slab() { return AABB{0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f}; }
    
    // Top slab (half height at top)
    static constexpr AABB top_slab() { return AABB{0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f}; }
};

/**
 * @brief Predefined block shapes for quick identification.
 * Used primarily for optimized collision detection.
 */
enum class BlockShape : std::uint8_t {
    Empty = 0,      // No collision (air, water)
    Full,           // Standard 1x1x1 block
    BottomSlab,     // Half block at bottom
    TopSlab,        // Half block at top
    Fence,          // Central post + conditional arms
    Wall,           // Similar to fence but different dimensions
    Stairs,         // L-shaped with orientation
    Cross,          // X-shaped (vegetation: tall grass, flowers)
    Custom          // Uses custom collision boxes from model
};

/**
 * @brief Face direction enum for UV mapping and face culling.
 */
enum class Face : std::uint8_t {
    East = 0,   // +X
    West,       // -X
    Up,         // +Y
    Down,       // -Y
    South,      // +Z
    North,      // -Z
    Count
};

/**
 * @brief A single cubic element within a block model.
 * Similar to Minecraft's block model "elements" array.
 * Coordinates are in 0-16 range (will be normalized to 0-1).
 */
struct ModelElement {
    // Origin and size in 0-16 coordinate space (like Minecraft)
    std::array<float, 3> from{0.0f, 0.0f, 0.0f};  // min corner
    std::array<float, 3> to{16.0f, 16.0f, 16.0f}; // max corner
    
    // Per-face texture references and UV coordinates
    struct FaceData {
        std::string texture;                        // Texture variable reference (e.g., "#top")
        std::array<float, 4> uv{0, 0, 16, 16};      // UV coordinates in 0-16 space
        int rotation{0};                            // UV rotation (0, 90, 180, 270)
        int tintIndex{-1};                          // Tint index for biome coloring (-1 = no tint)
        bool cullface{true};                        // Whether to cull when adjacent to solid block
    };
    
    // Face data indexed by Face enum (East, West, Up, Down, South, North)
    std::array<FaceData, 6> faces{};
    std::array<bool, 6> faceEnabled{false, false, false, false, false, false};
    
    // Element rotation (optional)
    std::array<float, 3> rotationOrigin{8.0f, 8.0f, 8.0f};
    char rotationAxis{'y'}; // 'x', 'y', or 'z'
    float rotationAngle{0.0f};
    bool rotationRescale{false};
    
    // Convert from to AABB (normalized 0-1)
    AABB to_aabb() const {
        return AABB{
            from[0] / 16.0f, from[1] / 16.0f, from[2] / 16.0f,
            to[0] / 16.0f, to[1] / 16.0f, to[2] / 16.0f
        };
    }
};

/**
 * @brief Complete block model definition.
 * Can be loaded from JSON (like Minecraft's block model format).
 */
struct BlockModel {
    std::string id;                     // Model identifier (e.g., "stone_slab")
    std::string parent;                 // Parent model to inherit from (optional)
    
    // Texture variable mappings (e.g., "top" -> "blocks/stone_slab_top")
    std::unordered_map<std::string, std::string> textures;
    
    // Model elements (cubes that make up the block)
    std::vector<ModelElement> elements;
    
    // Collision boxes for physics (may differ from visual elements)
    std::vector<AABB> collisionBoxes;
    
    // Quick shape classification for optimized collision
    BlockShape shape{BlockShape::Full};
    
    // Display transforms (for item rendering, etc.) - can be expanded later
    bool ambientOcclusion{true};
    
    // Helper to check if model has valid elements
    bool has_elements() const { return !elements.empty(); }
    
    // Helper to get total collision volume
    bool is_solid() const { return shape != BlockShape::Empty; }
};

/**
 * @brief Block state variant (for blocks with multiple visual states).
 * E.g., slabs can be "bottom" or "top", fences have connection states.
 */
struct BlockStateVariant {
    std::string model;       // Model ID to use
    int rotationX{0};        // X rotation (0, 90, 180, 270)
    int rotationY{0};        // Y rotation
    bool uvLock{false};      // Lock UVs during rotation
    int weight{1};           // Random selection weight
};

/**
 * @brief Block state definition mapping properties to model variants.
 */
struct BlockState {
    std::string blockId;
    
    // Simple variant (single model, optionally rotated)
    std::vector<BlockStateVariant> variants;
    
    // Multipart system (for complex blocks like fences)
    // Each part has a condition ("when") and applies one or more models
    struct MultipartCase {
        // Condition map (property -> required value)
        std::unordered_map<std::string, std::string> when;
        std::vector<BlockStateVariant> apply;
    };
    std::vector<MultipartCase> multipart;
};

// Predefined models for basic shapes
namespace models {

inline BlockModel make_full_block() {
    BlockModel m;
    m.shape = BlockShape::Full;
    m.collisionBoxes.push_back(AABB::full());
    
    ModelElement elem;
    elem.from = {0.0f, 0.0f, 0.0f};
    elem.to = {16.0f, 16.0f, 16.0f};
    for (int i = 0; i < 6; ++i) {
        elem.faceEnabled[i] = true;
        elem.faces[i].texture = "#all";
    }
    m.elements.push_back(elem);
    
    return m;
}

inline BlockModel make_bottom_slab() {
    BlockModel m;
    m.shape = BlockShape::BottomSlab;
    m.collisionBoxes.push_back(AABB::bottom_slab());
    
    ModelElement elem;
    elem.from = {0.0f, 0.0f, 0.0f};
    elem.to = {16.0f, 8.0f, 16.0f};
    for (int i = 0; i < 6; ++i) {
        elem.faceEnabled[i] = true;
        elem.faces[i].texture = "#all";
    }
    // Adjust UVs for sides
    elem.faces[static_cast<int>(Face::East)].uv = {0, 8, 16, 16};
    elem.faces[static_cast<int>(Face::West)].uv = {0, 8, 16, 16};
    elem.faces[static_cast<int>(Face::South)].uv = {0, 8, 16, 16};
    elem.faces[static_cast<int>(Face::North)].uv = {0, 8, 16, 16};
    m.elements.push_back(elem);
    
    return m;
}

inline BlockModel make_top_slab() {
    BlockModel m;
    m.shape = BlockShape::TopSlab;
    m.collisionBoxes.push_back(AABB::top_slab());
    
    ModelElement elem;
    elem.from = {0.0f, 8.0f, 0.0f};
    elem.to = {16.0f, 16.0f, 16.0f};
    for (int i = 0; i < 6; ++i) {
        elem.faceEnabled[i] = true;
        elem.faces[i].texture = "#all";
    }
    // Adjust UVs for sides
    elem.faces[static_cast<int>(Face::East)].uv = {0, 0, 16, 8};
    elem.faces[static_cast<int>(Face::West)].uv = {0, 0, 16, 8};
    elem.faces[static_cast<int>(Face::South)].uv = {0, 0, 16, 8};
    elem.faces[static_cast<int>(Face::North)].uv = {0, 0, 16, 8};
    m.elements.push_back(elem);
    
    return m;
}

inline BlockModel make_fence_post() {
    BlockModel m;
    m.shape = BlockShape::Fence;
    
    // Central post collision
    m.collisionBoxes.push_back(AABB{6.0f/16.0f, 0.0f, 6.0f/16.0f, 10.0f/16.0f, 1.0f, 10.0f/16.0f});
    
    // Visual post element
    ModelElement post;
    post.from = {6.0f, 0.0f, 6.0f};
    post.to = {10.0f, 16.0f, 10.0f};
    for (int i = 0; i < 6; ++i) {
        post.faceEnabled[i] = true;
        post.faces[i].texture = "#post";
    }
    m.elements.push_back(post);
    
    return m;
}

/**
 * @brief Creates fence connection elements based on BlockRuntimeState.
 * Returns vector of elements to render (post + connections).
 */
inline std::vector<ModelElement> make_fence_elements(bool north, bool south, bool east, bool west) {
    std::vector<ModelElement> elements;
    
    // Central post (always present)
    ModelElement post;
    post.from = {6.0f, 0.0f, 6.0f};
    post.to = {10.0f, 16.0f, 10.0f};
    for (int i = 0; i < 6; ++i) {
        post.faceEnabled[i] = true;
        post.faces[i].texture = "#post";
    }
    elements.push_back(post);
    
    // Connection bars (2 bars per direction: upper and lower)
    auto add_connection = [&](float fromX, float fromZ, float toX, float toZ) {
        // Lower bar (y: 6-9)
        ModelElement lower;
        lower.from = {fromX, 6.0f, fromZ};
        lower.to = {toX, 9.0f, toZ};
        for (int i = 0; i < 6; ++i) {
            lower.faceEnabled[i] = true;
            lower.faces[i].texture = "#post";
        }
        elements.push_back(lower);
        
        // Upper bar (y: 12-15)
        ModelElement upper;
        upper.from = {fromX, 12.0f, fromZ};
        upper.to = {toX, 15.0f, toZ};
        for (int i = 0; i < 6; ++i) {
            upper.faceEnabled[i] = true;
            upper.faces[i].texture = "#post";
        }
        elements.push_back(upper);
    };
    
    // North (-Z): from post to edge
    if (north) {
        add_connection(7.0f, 0.0f, 9.0f, 6.0f);
    }
    
    // South (+Z): from post to edge
    if (south) {
        add_connection(7.0f, 10.0f, 9.0f, 16.0f);
    }
    
    // West (-X): from edge to post
    if (west) {
        add_connection(0.0f, 7.0f, 6.0f, 9.0f);
    }
    
    // East (+X): from post to edge
    if (east) {
        add_connection(10.0f, 7.0f, 16.0f, 9.0f);
    }
    
    return elements;
}

/**
 * @brief Creates slab element based on SlabType (Top/Bottom/Double).
 */
inline ModelElement make_slab_element(SlabType slabType) {
    ModelElement elem;
    
    if (slabType == SlabType::Top) {
        elem.from = {0.0f, 8.0f, 0.0f};
        elem.to = {16.0f, 16.0f, 16.0f};
    } else if (slabType == SlabType::Double) {
        elem.from = {0.0f, 0.0f, 0.0f};
        elem.to = {16.0f, 16.0f, 16.0f};
    } else { // Bottom (default)
        elem.from = {0.0f, 0.0f, 0.0f};
        elem.to = {16.0f, 8.0f, 16.0f};
    }
    
    for (int i = 0; i < 6; ++i) {
        elem.faceEnabled[i] = true;
        elem.faces[i].texture = "#all";
    }
    
    return elem;
}

/**
 * @brief Creates a cross-shaped (X) model for vegetation like tall grass and flowers.
 * The model consists of two diagonal planes forming an X when viewed from above.
 * This is how Minecraft renders flowers and tall grass.
 */
inline BlockModel make_cross() {
    BlockModel m;
    m.shape = BlockShape::Cross;
    // No collision for vegetation
    m.ambientOcclusion = false;
    
    // Cross models use two diagonal planes at 45 degrees
    // We create custom elements that will be rendered specially
    // The actual rendering happens in chunk.cpp using specialized cross rendering
    
    return m;
}

} // namespace models

} // namespace shared::voxel
