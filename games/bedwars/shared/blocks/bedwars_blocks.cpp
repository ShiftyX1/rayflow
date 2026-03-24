#include "bedwars_blocks.hpp"
#include "engine/modules/voxel/shared/block_data.hpp"

using namespace shared::voxel;

namespace bedwars {

// ============================================================================
// Helper to register both shared data and (optionally) client registry data
// for a single block type in one call.
// ============================================================================

static void reg(BlockType type, const BlockData& data) {
    register_block_data(type, data);
}

// ============================================================================
// Shared block data (light + collision + category flags)
// ============================================================================

void register_block_shared_data() {
    // Air
    reg(BlockType::Air, {
        /* light */     {0u, 0u, 0u, false, false},
        /* collision */ {0.f, 1.f, 0.f, 0.f, 0.f, 1.f, false},
        /* solid */ false, /* transparent */ true, /* full_opaque */ false,
        /* vegetation */ false, /* slab */ false, /* fence */ false
    });

    // --- Solid natural blocks ---
    auto solid_opaque = [](BlockLightProps lp) {
        BlockData d{};
        d.light = lp;
        d.collision = {0.f, 1.f, 0.f, 1.f, 0.f, 1.f, true};
        d.is_solid = true;
        d.is_transparent = false;
        d.is_full_opaque = true;
        return d;
    };

    reg(BlockType::Stone,   solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Dirt,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Grass,   solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Sand,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Wood,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Bedrock, solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Gravel,  solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Coal,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Iron,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Gold,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::Diamond, solid_opaque({0u, 0u, 0u, true, false}));

    // Team blocks
    reg(BlockType::TeamRed,    solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::TeamBlue,   solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::TeamGreen,  solid_opaque({0u, 0u, 0u, true, false}));
    reg(BlockType::TeamYellow, solid_opaque({0u, 0u, 0u, true, false}));

    // Water — semi-transparent, sky-dimming
    reg(BlockType::Water, {
        {0u, 0u, 0u, false, true},
        {0.f, 1.f, 0.f, 0.f, 0.f, 1.f, false},
        false, true, false, false, false, false
    });

    // Leaves — solid but transparent, sky-dimming
    reg(BlockType::Leaves, {
        {0u, 0u, 0u, false, true},
        {0.f, 1.f, 0.f, 1.f, 0.f, 1.f, true},
        true, true, false, false, false, false
    });

    // Light (LS-1) — emits light, no collision
    reg(BlockType::Light, {
        {15u, 0u, 0u, false, false},
        {0.f, 1.f, 0.f, 0.f, 0.f, 1.f, false},
        false, true, false, false, false, false
    });

    // --- Slabs (partial blocks) ---
    auto make_slab = [](float minY, float maxY) {
        BlockData d{};
        d.light = {0u, 0u, 0u, false, false};
        d.collision = {0.f, 1.f, minY, maxY, 0.f, 1.f, true};
        d.is_solid = true;
        d.is_transparent = true; // adjacent faces visible
        d.is_full_opaque = false;
        d.is_slab = true;
        return d;
    };

    reg(BlockType::StoneSlab,    make_slab(0.0f, 0.5f));
    reg(BlockType::StoneSlabTop, make_slab(0.5f, 1.0f));
    reg(BlockType::WoodSlab,     make_slab(0.0f, 0.5f));
    reg(BlockType::WoodSlabTop,  make_slab(0.5f, 1.0f));

    // OakFence — post collision
    {
        BlockData d{};
        d.light = {0u, 0u, 0u, false, false};
        d.collision = {6.f/16.f, 10.f/16.f, 0.f, 1.5f, 6.f/16.f, 10.f/16.f, true};
        d.is_solid = true;
        d.is_transparent = true;
        d.is_full_opaque = false;
        d.is_fence = true;
        reg(BlockType::OakFence, d);
    }

    // --- Vegetation (cross-shaped, transparent, no collision) ---
    auto vegetation = []() {
        BlockData d{};
        d.light = {0u, 0u, 0u, false, false};
        d.collision = {0.f, 1.f, 0.f, 0.f, 0.f, 1.f, false};
        d.is_transparent = true;
        d.is_vegetation = true;
        return d;
    };

    reg(BlockType::TallGrass, vegetation());
    reg(BlockType::Poppy,     vegetation());
    reg(BlockType::Dandelion,  vegetation());
    reg(BlockType::DeadBush,   vegetation());
}

} // namespace bedwars
