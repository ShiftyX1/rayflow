# Minecraft-style lighting (0..15 block + sky) — strict spec

## Purpose
Replace the existing lighting model with a **Minecraft-like, monochrome, integer light** system:
- 16 discrete light levels: **0..15** (int)
- Two light channels:
  - **Block light** (emissive blocks)
  - **Sky light** (light from sky/void)
- Block brightness is derived from these light levels using a **brightness curve** (gamma/time-of-day).

This spec is authoritative for the lighting model and **supersedes**:
- `.github/instructions/lighting-baked.instructions.md`

Those documents may remain as optional/legacy renderer notes, but their *lighting model assumptions* must not be used.

## Hard constraints (must not break)
- **Layering:**
  - `games/*/client/` may compute lighting for rendering, but it must be based only on replicated/authoritative block state.
  - If lighting ever affects gameplay validation/rules, the **server** must compute it.
- **Server headless:** no raylib in `games/*/server/` or `games/*/shared/`.
- **Determinism:** given the same block grid + lighting parameters, lighting results must be identical.
- **No allocations in hot server tick paths:** any server-side lighting updates must reuse buffers/queues.

## Definitions
- **LightLevel:** integer in [0, 15].
- **BlockLight(x):** emissive/propagated light level at voxel position x.
- **SkyLight(x):** sky/propagated light level at voxel position x.
- **Brightness(x):** final scalar applied in shading, derived from max(BlockLight, SkyLight) via a curve.

### Neighbor model (strict)
- Light propagates only through the 6 axis-aligned neighbors (Von Neumann neighborhood).
- Each propagation step is one block along a single axis.
- Therefore the effective distance is **Manhattan distance** (L1), which matches the described "subtract along each axis" diagonal behavior.

## Light sources
- A block may have an **emission** level `emit ∈ [0,15]`.
- The source block itself has `BlockLight = max(BlockLight, emit)`.
- Propagation to neighbors decreases according to the attenuation rules below.

## Attenuation rules
Light propagation from a cell `p` to a neighbor `n` is computed as:

- If the traversed block blocks light completely: **no propagation**.
- Otherwise:
  - `candidate = light(p) - (1 + extraAttenuation)`
  - `light(n) = max(light(n), clamp(candidate, 0..15))`

Where:
- The base cost per step is always **1**.
- `extraAttenuation` is determined by the block that light is passing *through / into* (implementation must pick one convention and use it consistently for both channels).

### Block light transparency categories
Define per-block properties (in shared data, not ad-hoc in the renderer):
- **Opaque (blocks block light):** stops block light completely.
- **No-penalty transparent (passes block light):** `extraAttenuation = 0`.
  - Examples (as per spec): **glass, iron bars**.
- **Penalty transparent (attenuates block light):** `extraAttenuation ≥ 1`.
  - "All other transparent blocks" fall here unless explicitly whitelisted.
- **Leaves/cobweb:** do **not** add extra attenuation for **block light** compared to generic transparent behavior.

### Sky light behavior
- Sky light does **not** "get dimmer at night" by changing SkyLight levels; instead, **the brightness curve changes** with time-of-day.
- Sky light maximum level is **15**.

#### Vertical injection (required model)
- SkyLight is seeded from "open sky" above the world (or top-of-column) with level 15.
- In a fully open air column, SkyLight remains **15 all the way down**.
- Sky light only decreases when passing through blocks with `skyAttenuation > 0`.

#### Sky light attenuation categories
- **Opaque:** blocks sky light completely.
- **No-penalty transparent:** `skyAttenuation = 0` (e.g., glass, iron bars).
- **Penalty transparent:** `skyAttenuation ≥ 1`.
- **Leaves/cobweb (and water in Java):** specifically must cause vertical dimming:
  - Under these blocks, SkyLight decreases by **1 per block downward**, starting from the obstacle.

#### Horizontal spread
After vertical seeding, SkyLight must also spread sideways through air/transparent blocks using the same 6-neighbor rule, decreasing by **1 per step** plus any sky attenuation.

## Combining channels
For shading, define:
- `CombinedLight = max(BlockLight, SkyLight)`

Minecraft-style nuance:
- Block light sources commonly start at 14 (torch) while sky can be 15, which naturally makes sky slightly "stronger" at maximum.

## Brightness curve (render-only)
- Brightness is a function of `CombinedLight` and environment (time-of-day/gamma).
- The curve is allowed to change with time-of-day (night/day), but the **stored SkyLight values do not**.

This repo must implement the curve as a pure function with explicit parameters, e.g.:
- `brightness = BrightnessCurve(CombinedLight, timeOfDay, gamma)`

No other system may directly reinterpret raw light values.

## Data storage
- Each voxel stores **two 4-bit values** (block + sky).
- Packing is allowed and recommended:
  - `uint8_t blockNibble (0..15)`
  - `uint8_t skyNibble (0..15)`
  - Or a single `uint8_t` per channel, or packed into one `uint8_t` with 4/4 bits.

Storage must be decoupled from rendering (renderer consumes already-computed light).

## Update model (required)
Lighting must be incrementally updated when blocks change.

### Events that require relight
- Place/remove a block.
- Change a block's transparency category.
- Change a block's emission level.

### Incremental relight algorithm (required)
Implement a two-phase update similar to classic Minecraft relight:
- **Increase propagation** queue: when a light value can increase (new source, removed blocker).
- **Decrease propagation** queue: when a light value must be decreased (removed source, placed blocker).

Requirements:
- Work in **bounded volumes** (chunk-local + neighbor margins).
- Use fixed-capacity or reserved queues; do not allocate per update in hot paths.
- Must handle "light can flow around obstacles" by BFS over 6-neighbors.

## Rendering integration (strict)
- Rendering must use the computed light values; do not substitute client-only GI/shadows as a replacement for this model.
- Any stylized effects (e.g., baked AO) may only modulate the final shaded color and must not change the underlying LightLevel fields.

## Acceptance criteria
- Lighting is discrete 0..15 with block+sky channels.
- Diagonal examples match Manhattan attenuation (e.g., torch 14 yields 13 orthogonal, 12 diagonal on flat plane; and 11 for 1-down + 2-axis diagonal).
- Opaque blocks stop propagation; glass/iron bars do not add attenuation.
- Leaves/cobweb (and optionally water) attenuate **sky** vertically by 1 per step below the obstacle.
- Night/day affects only brightness curve, not SkyLight storage.

## Non-acceptance
- Colored lighting.
- Euclidean-distance falloff.
- Any approach that makes lighting depend on client-only world state if used for gameplay.

