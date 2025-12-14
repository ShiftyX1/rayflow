# Ray-marched lighting (client-only) — strict spec

## Purpose
Add **directional sun lighting with ray-marched voxel shadows** to the client renderer.

This feature is **render-only**. It must not affect gameplay, simulation, networking, validation, or authoritative state.

## Hard constraints (must not break)
- **Layering:** all implementation lives in `client/` (and/or `ui/debug/` for toggles). No includes from `server/`.
- **Server headless:** do not add `raylib` / `rlgl` includes to `server/` or `shared/`.
- **Authority:** lighting must not depend on client-reported positions as truth; it only uses the local camera for rendering.
- **No new gameplay truth:** no “lighting-based visibility” or damage logic.

## Scope (strict)
### In-scope (Phase RM-1)
- One **directional** light (“sun”) with:
  - Lambert diffuse
  - Ambient term
  - **Shadow term computed via ray marching through a voxel occupancy field**
- Applies to **voxel world chunk models** rendered by `voxel::Chunk` (`DrawModel` path).
- A debug toggle to enable/disable the effect at runtime (debug UI only).

### Out of scope (Phase RM-1)
- Point/spot lights, colored GI, emissive blocks, reflections.
- Temporal accumulation, denoisers, cascaded shadow maps.
- Screen-space effects (SSAO/SSGI) unless explicitly added later.
- Any protocol or server changes.

## Rendering integration points (required)
- Lighting shader must be bound via **raylib `Shader`** on the chunk model material:
  - `voxel::Chunk::generate_mesh()` sets `model_.materials[0].shader = <lighting_shader>`.
- Non-voxel meshes/models drawn by `ecs::RenderSystem` may remain unlit for RM-1.

## Voxel occupancy representation (required)
To ray-march in the fragment shader, the shader must have access to a **bounded voxel occupancy volume**.

### Representation
Preferred (if supported by the graphics backend):
- A 3D texture `R8` (or nearest supported single-channel format) storing occupancy:
  - 0 = empty/air
  - 255 = solid

Fallback (acceptable for RM-1 on raylib builds without 3D texture helpers):
- A 2D texture storing packed Z-slices:
  - `width = volume_size.x`
  - `height = volume_size.y * volume_size.z`
  - texel `(x, z*sizeY + y)` stores occupancy for voxel `(x, y, z)`.

In both cases, the volume represents a camera-centered region in **world block coordinates**.

### Coordinate mapping (strict)
- Define:
  - `volume_origin_ws` (world-space, float3) at the **min corner** of the volume in world units
  - `volume_size` (int3) in voxels (blocks)
  - `voxel_size_ws = 1.0` (one voxel = one world block)
- Convert a world-space point `p_ws` to texture coordinates:
  - `p_vs = (p_ws - volume_origin_ws) / voxel_size_ws`
  - `uv3 = (p_vs + 0.5) / volume_size` (center sampling)
- Sampling outside `[0, 1]` must be treated as **empty** (no occluder).

### Update policy (strict)
- The 3D texture must not be rebuilt every frame.
- It may update only when:
  - The camera/player crosses a voxel-aligned threshold (e.g., moved >= N blocks), OR
  - A chunk mesh rebuild occurs that touches the currently resident volume.
- Updates must be capped (configurable) to avoid hitches.

## Shadow ray marching (shader spec)
### Inputs (uniforms)
- `sun_dir_ws` (float3, normalized; points *from surface toward sun*)
- `sun_color` (float3)
- `ambient_color` (float3)
- `volume_origin_ws` (float3)
- `volume_size` (float3 or int3 encoded)
- `sampler3D voxel_occupancy` (preferred) OR a packed `sampler2D` (fallback)

### Per-fragment algorithm (strict)
1. Compute base albedo from atlas as current pipeline does.
2. Compute Lambert term: `ndl = max(dot(normal_ws, sun_dir_ws), 0)`.
3. Compute shadow by marching from a point slightly offset along normal:
   - `p0 = world_pos + normal_ws * 0.05`
   - For `i = 0..MAX_STEPS-1`:
     - `p = p0 + sun_dir_ws * (i * STEP_SIZE)`
     - Sample occupancy at `p`.
     - If occupied, set `shadow = 0` and stop.
   - If none hit, `shadow = 1`.
4. Final color:
   - `rgb = albedo * (ambient_color + sun_color * ndl * shadow)`

### Limits (required)
- `MAX_STEPS` must be a compile-time constant.
- Default values for RM-1:
  - `MAX_STEPS = 48`
  - `STEP_SIZE = 0.5` (world units)
- The shader must early-exit on the first occluder hit.

## Debug UI requirements
- Add a **debug-only** checkbox toggle:
  - `Lighting: Raymarch Shadows` (on/off)
- When disabled, rendering must fall back to current behavior.
- Debug UI must follow the rules in `.github/instructions/gui.instructions.md`.

## Configuration surface (client-only)
- Provide a client config (or constants) for:
  - volume size (e.g., 64×96×64)
  - update threshold
  - max update rate (Hz)
  - shader max steps

## Performance budgets (guidance; do not violate without justification)
- Shader: ≤ 48 steps per fragment (default).
- Volume uploads: must not exceed one full-volume upload more than ~2 times/sec.
- Avoid per-frame CPU scanning of all loaded chunks.

## Acceptance criteria (RM-1)
- No changes to `server/` and `shared/` are required.
- Voxel world still renders correctly when the feature is disabled.
- When enabled, solid blocks cast visible directional shadows on nearby surfaces.
- The toggle works at runtime and does not capture gameplay input unless using interactive debug UI mode.

## Non-acceptance (explicitly rejected)
- Any approach that requires server-side raylib/shader code.
- Any approach that makes lighting influence gameplay outcomes.
- Any approach that uploads the entire 3D occupancy volume every frame.
