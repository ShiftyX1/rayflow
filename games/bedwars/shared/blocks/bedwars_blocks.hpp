#pragma once

// =============================================================================
// BedWars block type definitions.
// All block data (light properties, collision, rendering info) that was
// previously hardcoded in engine/modules/voxel is now registered here.
// Called once during game initialization.
// =============================================================================

namespace bedwars {

/// Register all block shared data (light, collision, flags) into the engine's
/// runtime block data registry.  Must be called by both client and server.
void register_block_shared_data();

/// Register all block rendering data (textures, hardness, tool levels) into
/// the engine's BlockRegistry.  Client-only (requires GPU resources).
void register_block_client_data();

} // namespace bedwars
