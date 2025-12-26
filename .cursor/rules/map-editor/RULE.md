# Map templates + Map Editor tool — strict spec

## Purpose
Move BedWars from temporary seed terrain to **finite authored maps** loaded from **templates/prefabs**.

Provide a **separate tool** ("Map Editor") where a user can fly in Creative mode, place/remove blocks, and export a map template.

## Hard constraints (must not break)
- **Authoritative server + thin client** remains the architecture.
  - The editor is not a special offline codepath that bypasses validation.
  - The editor still uses the same transport abstraction (LocalTransport offline; NetTransport later).
- **No persistent worlds:** maps are templates, not saves. Match reset re-instantiates from the template.
- **Server headless:** map compilation/export must not require raylib in `server/`.
- **Client does not mutate authoritative world directly:** editor client sends intents; server applies changes.

## Map template definition
A map template is an asset that can be instantiated into an authoritative match.

### Required contents (MT-1)
- `mapId` (string)
- `version` (uint32)
- Bounds are **configured in chunks**:
  - `chunk_min` (int2)
  - `chunk_max` (int2)
  - The world is finite to this chunk AABB; outside it is treated as void/air (plus a kill/void rule handled by server).
- Block data is **sparse** (do not store air):
  - Store only non-Air blocks.
  - Block type IDs must be from `shared/voxel/block.hpp`.
- Protection metadata:
  - Which blocks are part of the template (protected)
  - Optional allow-list for breakable template blocks
- Gameplay anchors:
  - Team spawns
  - Bed positions (per team)
  - Shop locations
  - Generator locations
  - World boundary definition (must be present for MT-1; may match chunk bounds or be a separate rule)

### Optional contents (MT-1)
- Static/baked lighting data (see baked lighting spec)

### Storage format (MT-1)
- Define one canonical on-disk representation.
- Must be versioned.
- Must be loadable by the server with no rendering dependencies.

MT-1 recommendation (sufficiently strict to implement):
- File extension: `.rfmap`
- Header: magic + formatVersion
- Metadata: `mapId`, `version`
- Chunk bounds: `chunk_min`, `chunk_max`
- Chunk records: for each present chunk, store a list of non-Air blocks:
  - local coords `(lx, ly, lz)` + `blockType`
  - optional compression (RLE/varint) is allowed but not required for MT-1

Recommended direction (not a mandate):
- A compact binary "schematic-like" file OR a simple chunked format.

## Server responsibilities
- Loads a map template by `mapId`.
- Instantiates the map into the authoritative world state.
- Enforces protection rules:
  - Template blocks are protected by default.
  - Player-placed blocks are breakable by default.
- Provides editor permissions mode:
  - Allows place/break anywhere inside bounds (still validated and logged).

## Client responsibilities
- Renders the world and UI.
- Editor client UI provides:
  - creative fly controls
  - block palette selection
  - export command trigger
- Client never writes files as "truth"; it requests the server to export.

Editor UI constraint (required):
- Map editor UI must use **raygui only** (no XML UI).

## Protocol requirements (additive)
The editor must operate via protocol messages (same as gameplay).

Minimum additions:
- `TrySetBlock { seq, x,y,z, blockType }` (editor intent)
- `TryFillRegion { seq, min, max, blockType }` (optional)
- `TryExportMap { seq, mapId, options }` (server writes template)
- `ExportResult { seq, ok, reason?, path? }`

Rules:
- All commands are server-validated.
- Server logs accept/reject with reasons.

## Tooling layout (recommended)
- Add a separate executable target (e.g., `rayflow_map_editor`) under `app/`.
- It links client + server like the current main app does, but uses:
  - LocalTransport
  - Server "editor mode" configuration
  - Editor UI screens

## Editor startup flow (required)
The Map Editor must start with an initialization screen **before entering the 3D editor**.

### Init screen
The first screen provides two actions:
- **Create new map**
- **Open existing map**

### Create new map (required modal)
Before entering the editor, a modal dialog must collect these required parameters:
- `mapId` (string, required)
- `version` (uint32, required, must be > 0)
- `sizeXChunks` and `sizeZChunks` (int, required, must be > 0)

The chosen chunk size must be converted into export bounds:
- `chunkMinX = 0`, `chunkMinZ = 0`
- `chunkMaxX = sizeXChunks - 1`, `chunkMaxZ = sizeZChunks - 1`

### Open existing map
Before entering the editor, the user must be able to pick a `.rfmap` using a **SelectFileDialog**.
The dialog should be rooted at the runtime `maps/` directory (relative to process working directory) and filter to `.rfmap` files.
The editor loads the chosen map for rendering and uses its metadata (`mapId`, `version`, bounds) as defaults.

## Built-in map templates (required)
When creating a new map, the modal must allow choosing one of two templates:
1) **Floating island**: a simple single floating island made of blocks inside the selected bounds.
2) **Random chunks**: procedurally generated chunks inside the selected bounds.

Note: these templates are editor-side conveniences; authoritative state is still produced via server-applied edits.

No special "offline bypass" calls from client to server.

## Map visual settings (MV-1) — strict
Maps must carry **visual-only** environment settings that affect rendering in both the main client and the Map Editor.

### Scope (MV-1)
- Global lighting config for client shading (directional sun/moon preset + intensities).
- Skybox selection and rendering.

### Hard constraints
- Visual settings are **render-only**: they must not affect authoritative simulation, validation, damage, visibility, etc.
- Server remains headless and must not depend on raylib.
- The Map Editor must not directly write files as truth; settings are exported by the server as part of `.rfmap`.

### Data model (required)
`shared::maps::MapTemplate` must include a `VisualSettings` struct with at minimum:
- `skyboxKind` enum (uint8)
  - `0 = None`, `1 = Day`, `2 = Night` (MV-1)
- `timeOfDayHours` (float32, range [0, 24])
- `useMoon` (bool)
- `sunIntensity` (float32, range [0, 10])
- `ambientIntensity` (float32, range [0, 5])

MV-2 additive field:
- `temperature` (float32, range [0, 1], default 0.5)
  - Affects **only foliage/grass rendering color** for:
    - `BlockType::Leaves` (all faces)
    - `BlockType::Grass` (top face only)
  - The effect must be a **full recolor** (not a subtle tint)
  - Must not affect authoritative simulation.

### Serialization (required)
- `.rfmap` must store `VisualSettings` in the **section table** (format v2+).
- The section must be skippable/optional and forward-compatible.
- If a map is missing the section, defaults must be applied:
  - `skyboxKind = Day`
  - `timeOfDayHours = 12`, `useMoon = false`, `sunIntensity = 1`, `ambientIntensity = 0.25`

MV-2 backward/forward compatibility requirement:
- If a map has MV-1 `VisualSettings` only, `temperature` must default to `0.5`.
- Readers must tolerate larger payloads by skipping unknown tail bytes.

### Runtime application (required)
- When a template is loaded on the client (`world->set_map_template()`), the client must apply `VisualSettings`:
  - Configure global light using the map values.
  - Configure and render the selected skybox.
  - Apply `temperature` so grass/foliage tint matches the map.
- The Map Editor must also apply these settings while editing.

### Editor UI (required)
- The Map Editor must provide controls to edit `VisualSettings` in-editor.
- On export, the current visual settings must be included in the exported `.rfmap`.

## Light sources (LS-1) — strict
Maps must support placing **light source markers**.

### Representation (required)
- Light sources are represented as blocks using a shared block ID:
  - Add `shared::voxel::BlockType::Light` (append-only; keep existing IDs stable).

### Physics rules (required)
- `BlockType::Light` must be **non-solid** (no collision) and treated as **transparent** for meshing.
- Light blocks must not affect baked AO in BL-1 (they are markers only for now).

### Rendering rules (required)
- Light blocks must be visible in the Map Editor.
- Because they may not have atlas tiles, they may be rendered via a distinct marker (e.g., small emissive sphere/billboard) instead of a textured cube.
- Light blocks may be visible in the main client as the same marker.

## Creative/editor movement
- Editor movement is still a client intent (`InputFrame`) and server-authoritative.
- Creative fly rules can be enabled server-side for editor sessions.

## Bake integration
- The Map Editor tool is the place to trigger:
  - "Bake static lighting" (offline) and embed results into the template.
- Baking must not run inside the server tick loop.

## Acceptance criteria (MT-1)
- Server can load a map template and start a match on it.
- Match reset fully restores the template.
- Map Editor can:
  - place/break blocks (through server validation)
  - set required anchors (spawns/beds/shops/generators)
  - export a template
- Exported template can be loaded by the main game.

Export requirements (MT-1):
- Export must require both `mapId` and `version`.
- Server writes the map file at runtime (not a source-only build step).

Runtime export location (MT-1):
- Server must write exported maps to a runtime directory `maps/` relative to the process working directory (same folder level as `textures/` and `ui/` in the current build layout).
- Server must create `maps/` if it does not exist.
- Filename convention (required): `maps/<mapId>_v<version>.rfmap`.

## Non-acceptance
- Any editor that directly mutates `server::World` from client code without transport.
- Any editor that depends on raylib in `server/`.
- Any design that turns templates into persistent world saves.

