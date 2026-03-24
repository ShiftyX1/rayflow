#include "shared/blocks/bedwars_blocks.hpp"
#include "engine/modules/voxel/shared/block.hpp"
#include "engine/modules/voxel/client/block_registry.hpp"

using namespace shared::voxel;

namespace bedwars {

void register_block_client_data() {
    using voxel::BlockInfo;
    auto& r = voxel::BlockRegistry::instance();

    r.register_block(BlockType::Air,     {"Air",       false, true,   0.0f, 0, {0,0,0,0,0,0}});
    r.register_block(BlockType::Stone,   {"Stone",     true,  false,  1.5f, 1, {1,1,1,1,1,1}});
    r.register_block(BlockType::Dirt,    {"Dirt",      true,  false,  0.5f, 0, {2,2,2,2,2,2}});
    r.register_block(BlockType::Grass,   {"Grass",     true,  false,  0.6f, 0, {3,3,0,2,3,3}});
    r.register_block(BlockType::Sand,    {"Sand",      true,  false,  0.5f, 0, {18,18,18,18,18,18}});
    r.register_block(BlockType::Water,   {"Water",     false, true, 100.0f, 0, {205,205,205,205,205,205}});
    r.register_block(BlockType::Wood,    {"Wood",      true,  false,  2.0f, 0, {20,20,21,21,20,20}});
    r.register_block(BlockType::Leaves,  {"Leaves",    true,  true,   0.2f, 0, {52,52,52,52,52,52}});
    r.register_block(BlockType::Bedrock, {"Bedrock",   true,  false, -1.0f, 255, {17,17,17,17,17,17}});
    r.register_block(BlockType::Gravel,  {"Gravel",    true,  false,  0.6f, 0, {19,19,19,19,19,19}});
    r.register_block(BlockType::Coal,    {"Coal Ore",  true,  false,  3.0f, 1, {34,34,34,34,34,34}});
    r.register_block(BlockType::Iron,    {"Iron Ore",  true,  false,  3.0f, 2, {33,33,33,33,33,33}});
    r.register_block(BlockType::Gold,    {"Gold Ore",  true,  false,  3.0f, 3, {32,32,32,32,32,32}});
    r.register_block(BlockType::Diamond, {"Diamond Ore", true, false, 3.0f, 3, {50,50,50,50,50,50}});
    r.register_block(BlockType::Light,   {"Light",     false, true,   0.0f, 0, {0,0,0,0,0,0}});

    r.register_block(BlockType::StoneSlab,    {"Stone Slab",     true, false, 1.5f, 1, {1,1,1,1,1,1}});
    r.register_block(BlockType::StoneSlabTop, {"Stone Slab Top", true, false, 1.5f, 1, {1,1,1,1,1,1}});
    r.register_block(BlockType::WoodSlab,     {"Wood Slab",      true, false, 2.0f, 0, {4,4,4,4,4,4}});
    r.register_block(BlockType::WoodSlabTop,  {"Wood Slab Top",  true, false, 2.0f, 0, {4,4,4,4,4,4}});
    r.register_block(BlockType::OakFence,     {"Oak Fence",      true, false, 2.0f, 0, {4,4,4,4,4,4}});

    r.register_block(BlockType::TallGrass, {"Tall Grass", false, true, 0.0f, 0, {39,39,39,39,39,39}});
    r.register_block(BlockType::Poppy,     {"Poppy",      false, true, 0.0f, 0, {12,12,12,12,12,12}});
    r.register_block(BlockType::Dandelion, {"Dandelion",   false, true, 0.0f, 0, {13,13,13,13,13,13}});
    r.register_block(BlockType::DeadBush,  {"Dead Bush",   false, true, 0.0f, 0, {55,55,55,55,55,55}});

    r.register_block(BlockType::TeamRed,    {"Team Red",    true, false, 50.0f, 0, {240,240,240,240,240,240}});
    r.register_block(BlockType::TeamBlue,   {"Team Blue",   true, false, 50.0f, 0, {241,241,241,241,241,241}});
    r.register_block(BlockType::TeamGreen,  {"Team Green",  true, false, 50.0f, 0, {242,242,242,242,242,242}});
    r.register_block(BlockType::TeamYellow, {"Team Yellow", true, false, 50.0f, 0, {243,243,243,243,243,243}});
}

} // namespace bedwars
