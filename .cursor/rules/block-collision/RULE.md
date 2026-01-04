# Block Collision System

## Overview
The block collision system handles physics interactions between players and voxel blocks with non-standard shapes (slabs, fences, etc.). This system is **server-authoritative** — all collision calculations happen on the server.

## Key Files
- `engine/modules/voxel/block.hpp` — `BlockCollisionInfo` struct and `get_collision_info()` function
- `engine/modules/voxel/block_shape.hpp` — `AABB`, `BlockModel`, `BlockShape` types
- `games/bedwars/server/` — collision resolution functions (server-side)
- `games/bedwars/client/voxel/block_model_loader.cpp` — JSON model parsing including collision boxes
- `games/bedwars/resources/models/block/*.json` — block model definitions with collision data

## Collision Info Structure

```cpp
struct BlockCollisionInfo {
    float minX, maxX;  // X bounds [0.0, 1.0] relative to block
    float minY, maxY;  // Y bounds (can exceed 1.0 for tall collision like fences)
    float minZ, maxZ;  // Z bounds [0.0, 1.0] relative to block
    bool hasCollision; // Whether block has any collision
};
```

### Collision Height Rules
- **Standard blocks**: maxY = 1.0
- **Bottom slabs**: maxY = 0.5 (allows step-up)
- **Top slabs**: minY = 0.5, maxY = 1.0
- **Fences**: maxY = 1.5 (prevents jumping over)

## Step-Up Mechanic

### Purpose
Players can automatically walk onto blocks with collision height ≤ 0.5 (like slabs) without needing to jump, similar to Minecraft.

### Implementation
```cpp
constexpr float kMaxStepUpHeight = 0.5f + kEps;
```

### Step-Up Algorithm (in movement loop)
1. Apply horizontal movement (X or Z)
2. Run collision resolution
3. If player was blocked AND is on ground:
   - Temporarily restore intended position
   - Calculate obstacle height at feet level via `get_obstacle_step_height()`
   - If height ≤ `kMaxStepUpHeight` AND there's headroom above:
     - Elevate player by step height
     - Allow horizontal movement
   - Else: revert position, zero velocity

### Key Functions
- `get_obstacle_step_height()` — finds max collision height at player's feet position
- `try_step_up()` — attempts step-up, checking headroom (legacy, now inline)

## Tall Collision (Fences)

### Problem
Fences have collision height of 1.5 blocks but only occupy Y=0 to Y=1 in block coordinates. When player jumps (reaches Y > 1.0), naive collision check misses the fence because it only checks blocks at player's Y level.

### Solution
Collision resolution functions (`resolve_voxel_x`, `resolve_voxel_z`) check blocks starting from `min_y - 1`:

```cpp
int min_y = fast_floor_to_int(py + kEps) - 1;
if (min_y < 0) min_y = 0;
```

This ensures blocks with tall collision (maxY > 1.0) still block the player.

## JSON Model Collision Format

### Location
`games/bedwars/resources/models/block/<block_id>.json`

### Collision Array Format
```json
{
    "collision": [
        [minX, minY, minZ, maxX, maxY, maxZ]
    ]
}
```
Coordinates are in Minecraft-style 0-16 space (will be normalized to 0-1).

### Examples

**Fence (1.5 block collision height):**
```json
{
    "collision": [[6, 0, 6, 10, 24, 10]]
}
```
Note: 24/16 = 1.5 block height

**Bottom Slab:**
```json
{
    "collision": [[0, 0, 0, 16, 8, 16]]
}
```
Note: 8/16 = 0.5 block height

**Top Slab:**
```json
{
    "collision": [[0, 8, 0, 16, 16, 16]]
}
```

## Server Collision Constants

Located in both `server.cpp` and `dedicated_server.cpp`:

```cpp
constexpr float kPlayerWidth = 0.6f;   // from shared::kPlayerWidth
constexpr float kPlayerHeight = 1.8f;  // from shared::kPlayerHeight
constexpr float kEps = 1e-4f;          // epsilon for float comparisons
constexpr float kSkin = 1e-3f;         // skin width to prevent clipping
constexpr float kMaxStepUpHeight = 0.5f + kEps;
```

## Hard Rules

### Server Authority
- All collision calculations happen on server
- Client does NOT run player physics
- Client only renders authoritative positions from `StateSnapshot`

### Block Registration
- New block types with custom collision MUST be added to `get_collision_info()` in `engine/modules/voxel/block.hpp`
- JSON collision is parsed on client for rendering but server uses hardcoded values

### Collision Bounds
- XZ bounds are always in [0.0, 1.0] range (within single block)
- Y bounds can exceed 1.0 for tall collision (fences, walls)
- Use `std::min(coll.maxY, 1.0f)` only for ground height calculations, NOT for collision checks

### Movement Resolution Order
1. Apply X movement → resolve X collision (with step-up)
2. Apply Z movement → resolve Z collision (with step-up)  
3. Apply Y movement → resolve Y collision (ground/ceiling)

## Adding New Block Types with Custom Collision

1. Add `BlockType` enum value in `engine/modules/voxel/block.hpp`
2. Add collision info to `get_collision_info()` switch statement
3. Create JSON model file in `games/bedwars/resources/models/block/`
4. Add `"collision"` array to JSON with bounds in 0-16 space
5. Register model in `BlockModelLoader::register_builtin_models()` if needed

## Debugging

Enable collision logging in server options:
```cpp
opts.logging.coll = true;
```

Log messages include:
- `X clamp+/clamp-` — blocked by collision in X direction
- `Z clamp+/clamp-` — blocked by collision in Z direction  
- `step-up X/Z` — successful step-up with height
- `landed block` — player landed on block surface
- `ceiling block` — player hit ceiling
