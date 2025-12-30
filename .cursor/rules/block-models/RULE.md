# Block Model System

## Overview
The block model system provides a Minecraft-compatible way to define block appearances and shapes using JSON files. This enables adding new block types without code changes and supports model inheritance, custom shapes, and per-face textures.

## Architecture

```
shared/voxel/block_shape.hpp    ← Data structures (AABB, BlockModel, ModelElement)
shared/voxel/block.hpp          ← BlockType enum, collision info
client/voxel/block_model_loader.hpp/cpp ← JSON parser and model registry
client/static/models/block/*.json       ← Model definitions
```

## Key Types

### BlockShape Enum
Quick shape classification for optimized collision/rendering:
```cpp
enum class BlockShape : std::uint8_t {
    Empty = 0,      // No collision (air, water)
    Full,           // Standard 1x1x1 block
    BottomSlab,     // Half block at bottom
    TopSlab,        // Half block at top  
    Fence,          // Central post + conditional arms
    Wall,           // Similar to fence
    Stairs,         // L-shaped with orientation
    Custom          // Uses custom collision boxes
};
```

### AABB (Axis-Aligned Bounding Box)
```cpp
struct AABB {
    float minX, minY, minZ;  // [0,1] range, block-local
    float maxX, maxY, maxZ;
    
    static constexpr AABB full();        // 0,0,0 → 1,1,1
    static constexpr AABB bottom_slab(); // 0,0,0 → 1,0.5,1
    static constexpr AABB top_slab();    // 0,0.5,0 → 1,1,1
};
```

### Face Enum
```cpp
enum class Face : std::uint8_t {
    East = 0,   // +X
    West,       // -X
    Up,         // +Y
    Down,       // -Y
    South,      // +Z
    North,      // -Z
    Count
};
```

### ModelElement
A single cubic element within a block model:
```cpp
struct ModelElement {
    std::array<float, 3> from;  // Min corner in 0-16 space
    std::array<float, 3> to;    // Max corner in 0-16 space
    
    struct FaceData {
        std::string texture;     // Texture variable (e.g., "#top")
        std::array<float, 4> uv; // UV coords in 0-16 space
        int rotation;            // UV rotation (0, 90, 180, 270)
        int tintIndex;           // Biome coloring (-1 = none)
        bool cullface;           // Cull when adjacent to solid
    };
    std::array<FaceData, 6> faces;
    std::array<bool, 6> faceEnabled;
    
    // Optional rotation
    std::array<float, 3> rotationOrigin;
    char rotationAxis;  // 'x', 'y', 'z'
    float rotationAngle;
    bool rotationRescale;
    
    AABB to_aabb() const;  // Convert to normalized AABB
};
```

### BlockModel
Complete block model definition:
```cpp
struct BlockModel {
    std::string id;                      // Model ID (e.g., "stone_slab")
    std::string parent;                  // Parent model for inheritance
    
    std::unordered_map<std::string, std::string> textures;  // Texture variables
    std::vector<ModelElement> elements;                      // Visual elements
    std::vector<AABB> collisionBoxes;                       // Physics collision
    
    BlockShape shape;           // Quick shape classification
    bool ambientOcclusion;      // AO enabled
};
```

## JSON Model Format

### Location
`client/static/models/block/<model_id>.json`

### Basic Structure
```json
{
    "parent": "block/block",
    "textures": {
        "all": "blocks/stone",
        "top": "blocks/stone_top",
        "side": "blocks/stone_side"
    },
    "collision": [
        [minX, minY, minZ, maxX, maxY, maxZ]
    ],
    "ambientocclusion": true,
    "elements": [
        {
            "from": [0, 0, 0],
            "to": [16, 16, 16],
            "faces": {
                "down":  {"texture": "#all", "uv": [0, 0, 16, 16]},
                "up":    {"texture": "#top", "uv": [0, 0, 16, 16]},
                "north": {"texture": "#side", "uv": [0, 0, 16, 16]},
                "south": {"texture": "#side", "uv": [0, 0, 16, 16]},
                "west":  {"texture": "#side", "uv": [0, 0, 16, 16]},
                "east":  {"texture": "#side", "uv": [0, 0, 16, 16]}
            }
        }
    ]
}
```

### Coordinate System
- All coordinates are in **0-16 space** (Minecraft convention)
- Converted to 0-1 normalized space internally (divide by 16)
- Origin (0,0,0) is the block's minimum corner

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `parent` | string | Parent model ID for inheritance |
| `textures` | object | Texture variable definitions |
| `collision` | array | Collision boxes `[[x1,y1,z1,x2,y2,z2], ...]` |
| `ambientocclusion` | bool | Enable ambient occlusion |
| `elements` | array | Visual elements (cubes) |

### Element Fields

| Field | Type | Description |
|-------|------|-------------|
| `from` | [x,y,z] | Minimum corner (0-16 space) |
| `to` | [x,y,z] | Maximum corner (0-16 space) |
| `faces` | object | Per-face data (down/up/north/south/west/east) |
| `rotation` | object | Optional element rotation |

### Face Fields

| Field | Type | Description |
|-------|------|-------------|
| `texture` | string | Texture variable reference (e.g., `"#all"`) |
| `uv` | [u1,v1,u2,v2] | UV coordinates (0-16 space) |
| `rotation` | int | UV rotation (0, 90, 180, 270) |
| `tintindex` | int | Biome color index (-1 = none) |
| `cullface` | bool | Face direction for culling |

### Element Rotation

```json
{
    "rotation": {
        "origin": [8, 8, 8],
        "axis": "y",
        "angle": 45,
        "rescale": false
    }
}
```

## Model Examples

### Full Block
```json
{
    "textures": { "all": "blocks/stone" },
    "elements": [{
        "from": [0, 0, 0],
        "to": [16, 16, 16],
        "faces": {
            "down":  {"texture": "#all"},
            "up":    {"texture": "#all"},
            "north": {"texture": "#all"},
            "south": {"texture": "#all"},
            "west":  {"texture": "#all"},
            "east":  {"texture": "#all"}
        }
    }]
}
```

### Bottom Slab
```json
{
    "textures": { "all": "blocks/stone" },
    "collision": [[0, 0, 0, 16, 8, 16]],
    "elements": [{
        "from": [0, 0, 0],
        "to": [16, 8, 16],
        "faces": {
            "down":  {"texture": "#all", "cullface": true},
            "up":    {"texture": "#all"},
            "north": {"texture": "#all", "uv": [0, 8, 16, 16], "cullface": true},
            "south": {"texture": "#all", "uv": [0, 8, 16, 16], "cullface": true},
            "west":  {"texture": "#all", "uv": [0, 8, 16, 16], "cullface": true},
            "east":  {"texture": "#all", "uv": [0, 8, 16, 16], "cullface": true}
        }
    }]
}
```

### Fence Post (Tall Collision)
```json
{
    "ambientocclusion": false,
    "textures": { "texture": "blocks/planks_oak" },
    "collision": [[6, 0, 6, 10, 24, 10]],
    "elements": [
        {
            "comment": "Post",
            "from": [6, 0, 6],
            "to": [10, 16, 10],
            "faces": {
                "down":  {"texture": "#texture", "uv": [6, 6, 10, 10]},
                "up":    {"texture": "#texture", "uv": [6, 6, 10, 10]},
                "north": {"texture": "#texture", "uv": [6, 0, 10, 16]},
                "south": {"texture": "#texture", "uv": [6, 0, 10, 16]},
                "west":  {"texture": "#texture", "uv": [6, 0, 10, 16]},
                "east":  {"texture": "#texture", "uv": [6, 0, 10, 16]}
            }
        }
    ]
}
```

Note: `collision: [[6, 0, 6, 10, 24, 10]]` means Y extends to 24/16 = 1.5 blocks.

## BlockModelLoader

### Singleton Access
```cpp
auto& loader = voxel::BlockModelLoader::instance();
```

### Initialization
```cpp
// Call once at startup
loader.init("models/block");
```

### Getting Models
```cpp
// By BlockType enum
const auto* model = loader.get_model(BlockType::StoneSlab);

// By string ID
const auto* model = loader.get_model_by_id("stone_slab");
```

### Registering Programmatic Models
```cpp
auto model = shared::voxel::models::make_bottom_slab();
model.id = "custom_slab";
model.textures["all"] = "blocks/custom";
loader.register_model(BlockType::CustomSlab, model);
```

## Model Inheritance

Models can inherit from parent models using the `parent` field:

```json
{
    "parent": "block/slab",
    "textures": {
        "all": "blocks/brick"
    }
}
```

Inheritance rules:
1. Child textures **override** parent textures
2. Child elements **replace** parent elements if present
3. Child collision boxes **replace** parent if present
4. Shape is inherited if child doesn't specify

## Adding New Block Types

### Step 1: Add BlockType Enum
In `shared/voxel/block.hpp`:
```cpp
enum class BlockType : std::uint8_t {
    // ...existing types...
    MyNewBlock,
    Count
};
```

### Step 2: Add Collision Info
In `shared/voxel/block.hpp` `get_collision_info()`:
```cpp
case BlockType::MyNewBlock:
    return {0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 1.0f, true};  // Example: bottom slab
```

### Step 3: Create JSON Model
Create `client/static/models/block/my_new_block.json`:
```json
{
    "textures": { "all": "blocks/my_texture" },
    "collision": [[0, 0, 0, 16, 8, 16]],
    "elements": [...]
}
```

### Step 4: Register Model
In `BlockModelLoader::register_builtin_models()`:
```cpp
{
    auto model = models::make_bottom_slab();
    model.id = "my_new_block";
    model.textures["all"] = "blocks/my_texture";
    register_model(BlockType::MyNewBlock, model);
}
```

### Step 5: Add Texture
Place texture at `textures/blocks/my_texture.png` or add to atlas.

## Shape Auto-Detection

If `collision` is not specified in JSON, the loader auto-detects shape from elements:

| Condition | Detected Shape |
|-----------|---------------|
| No elements | `Empty` |
| Full 16x16x16 cube | `Full` |
| Full XZ, Y from 0 to ≤8 | `BottomSlab` |
| Full XZ, Y from ≥8 to 16 | `TopSlab` |
| Narrow XZ (< 8 wide) | `Fence` |
| Multiple elements | `Custom` |

## Predefined Model Factories

In `shared::voxel::models`:
```cpp
BlockModel make_full_block();    // 16x16x16 cube
BlockModel make_bottom_slab();   // 16x8x16 at bottom
BlockModel make_top_slab();      // 16x8x16 at top
BlockModel make_fence_post();    // 4x16x4 central post
```

## Hard Rules

### Server vs Client
- **Server** uses `get_collision_info()` from `shared/voxel/block.hpp` (hardcoded)
- **Client** uses JSON models for rendering via `BlockModelLoader`
- Both must stay in sync for correct gameplay

### Coordinate Consistency
- Always use 0-16 space in JSON files
- Internal code uses 0-1 normalized space
- Collision bounds can exceed 1.0 for Y (tall blocks like fences)

### Texture Variables
- Start with `#` when referencing (e.g., `"#all"`)
- Resolve to paths like `"blocks/stone"`
- Must be defined in `textures` object or inherited from parent

### File Naming
- Model file name = model ID (e.g., `stone_slab.json` → ID `"stone_slab"`)
- Use lowercase with underscores
- Match `BlockType` enum naming when possible

---

## Block States and Connections

### Overview
Some blocks have multiple visual/collision states based on context:
- **Fences/Walls** — connect to adjacent fences/solid blocks
- **Slabs** — can merge into full blocks
- **Glass Panes** — connect like fences
- **Stairs** — corner shapes based on neighbors

### Block State System

Block states are property combinations that determine which model variant to use.

**Two separate systems exist:**
1. `BlockRuntimeState` (in `shared/voxel/block_state.hpp`) — compact runtime representation for networking/physics
2. JSON blockstates (in `client/static/blockstates/*.json`) — maps properties to model variants for rendering

```cpp
// Runtime state for physics/networking (shared/voxel/block_state.hpp)
struct BlockRuntimeState {
    bool north : 1;  // Connection flags for fences/walls
    bool south : 1;
    bool east  : 1;
    bool west  : 1;
    SlabType slabType : 2;  // Bottom, Top, Double
    // Serializes to single byte via to_byte()/from_byte()
};
```

### JSON Block State Definitions

Location: `client/static/blockstates/<block_id>.json`

#### Fence Example
```json
{
    "multipart": [
        {
            "apply": { "model": "block/oak_fence_post" }
        },
        {
            "when": { "north": true },
            "apply": { "model": "block/oak_fence_side", "uvlock": true }
        },
        {
            "when": { "south": true },
            "apply": { "model": "block/oak_fence_side", "y": 180, "uvlock": true }
        },
        {
            "when": { "east": true },
            "apply": { "model": "block/oak_fence_side", "y": 90, "uvlock": true }
        },
        {
            "when": { "west": true },
            "apply": { "model": "block/oak_fence_side", "y": 270, "uvlock": true }
        }
    ]
}
```

#### Slab Example (with double state)
```json
{
    "variants": {
        "type=bottom": { "model": "block/stone_slab" },
        "type=top": { "model": "block/stone_slab_top" },
        "type=double": { "model": "block/stone" }
    }
}
```

### Connection Logic

#### Fence Connection Rules
A fence connects to a neighbor if:
1. Neighbor is the same fence type, OR
2. Neighbor is a solid full block, OR
3. Neighbor is a compatible connecting block (wall, glass pane)

```cpp
// Pseudo-code for fence connection check
bool should_fence_connect(BlockPos pos, Face direction) {
    BlockPos neighbor = pos + direction_offset(direction);
    BlockType neighborType = world.get_block(neighbor);
    
    if (is_fence(neighborType)) return true;
    if (is_solid_full_block(neighborType)) return true;
    if (is_wall(neighborType) || is_glass_pane(neighborType)) return true;
    return false;
}
```

#### Collision for Connected Fences
When fences connect, collision boxes are added for the arms:

```cpp
// Fence post collision (always present)
AABB post = {6/16, 0, 6/16, 10/16, 24/16, 10/16};  // 1.5 blocks tall

// North arm collision (when north=true)
AABB north_arm = {6/16, 0, 0, 10/16, 24/16, 6/16};

// Full fence collision = post + enabled arms
std::vector<AABB> get_fence_collision(BlockRuntimeState state) {
    std::vector<AABB> boxes = {post};
    if (state.north) boxes.push_back(north_arm);
    if (state.south) boxes.push_back(south_arm);
    if (state.east) boxes.push_back(east_arm);
    if (state.west) boxes.push_back(west_arm);
    return boxes;
}
```

---

## Block Merging (Slab Doubling)

### Overview
Two slabs of the same type placed together form a double slab (full block):
- Bottom slab + place on top → Double slab
- Top slab + place on bottom → Double slab

### Placement Logic

```cpp
// Pseudo-code for slab placement
BlockPlacementResult place_slab(BlockPos target, Face clickedFace, 
                                 float hitY, BlockType slabType) {
    BlockType existing = world.get_block(target);
    
    // Case 1: Clicking on existing slab of same type
    if (is_same_slab_type(existing, slabType)) {
        SlabType existingSlabType = get_slab_type(existing);
        
        // Bottom slab + clicking top half → merge
        if (existingSlabType == SlabType::Bottom && hitY > 0.5f) {
            return {target, slabType, SlabType::Double};
        }
        // Top slab + clicking bottom half → merge  
        if (existingSlabType == SlabType::Top && hitY <= 0.5f) {
            return {target, slabType, SlabType::Double};
        }
    }
    
    // Case 2: Placing on empty space or different block
    BlockPos placePos = get_placement_position(target, clickedFace);
    
    // Determine bottom or top based on click position
    if (clickedFace == Face::Up || (clickedFace != Face::Down && hitY <= 0.5f)) {
        return {placePos, slabType, SlabType::Bottom};
    } else {
        return {placePos, slabType, SlabType::Top};
    }
}
```

### Double Slab Properties
- Uses full block collision (1x1x1)
- Uses full block model (or dedicated double_slab model)
- Drops 2 slabs when broken
- May have different texture for double variant

### Slab Type Mapping

| Slab Type | BlockType Enum | Model | Collision Height |
|-----------|---------------|-------|------------------|
| Stone (bottom) | `StoneSlab` | `stone_slab` | 0.5 |
| Stone (top) | `StoneSlabTop` | `stone_slab_top` | 0.5 (offset) |
| Stone (double) | `Stone` or `StoneSlabDouble` | `stone` | 1.0 |

### Implementation Options

#### Option A: Separate BlockTypes
```cpp
enum class BlockType {
    StoneSlab,        // Bottom slab
    StoneSlabTop,     // Top slab  
    StoneSlabDouble,  // Double slab (or just Stone)
};
```

#### Option B: Single BlockType + State
```cpp
// BlockType::StoneSlab with state property
struct SlabState {
    enum Type { Bottom, Top, Double } type;
};
```

**Recommendation**: Option B is cleaner for many slab materials but requires proper block state system.

---

## Block Interaction Rules

### Server-Side Validation
All placement/merging logic runs on server:

```cpp
// Server validates placement intent
bool validate_block_place(PlayerId player, BlockPos pos, 
                          BlockType type, BlockRuntimeState state) {
    // Check permissions
    // Check distance
    // Check placement rules (can merge, can connect, etc.)
    // Apply change and broadcast
}
```

### Client Prediction (Future Stage C)
Client may optimistically show connection/merge for responsiveness, but server is authoritative.

### Update Propagation
When a block changes, neighbors may need state updates:

```cpp
void on_block_changed(BlockPos pos) {
    // Update this block's connections
    update_block_state(pos);
    
    // Notify neighbors to update their connections
    for (Face face : all_faces) {
        BlockPos neighbor = pos + face_offset(face);
        if (needs_connection_update(neighbor)) {
            update_block_state(neighbor);
        }
    }
}
```

---

## Adding Connectable Blocks

### Step 1: Define Connection Group
```cpp
enum class ConnectionGroup {
    None,
    Fence,      // Connects to fences, solid blocks
    Wall,       // Connects to walls, fences, solid blocks
    GlassPane,  // Connects to panes, solid blocks
    Redstone,   // Special redstone connectivity
};
```

### Step 2: Add Block State Properties
In block definition, specify which properties this block uses:
```cpp
BlockStateDefinition fence_state = {
    .properties = {"north", "south", "east", "west"},
    .connectionGroup = ConnectionGroup::Fence,
};
```

### Step 3: Create Model Variants
Create models for each state combination or use multipart system:
- `oak_fence_post.json` — center post (always rendered)
- `oak_fence_side.json` — connecting arm (rendered per-direction)

### Step 4: Define Connection-Aware Collision
Return collision boxes based on current state, not just block type.
