# BedWars gameplay systems (server-owned)

All gameplay below must live in `server/` and be exposed to clients only via protocol messages.

## Core systems
- Teams, team spawns, team inventory rules.
- Beds: per-team bed object with destruction rules.
  - If bed is alive: players of that team can respawn.
  - If bed is destroyed: no respawn (death = elimination) or spectate, depending on design.

## Resource generators
- Timed generators (iron/gold/emerald/diamond etc.)
- Server tick-based timers.
- Drops/items are server-spawned entities and replicated.

## Shop + upgrades
- Server validates purchases and applies results:
  - Give items, deduct currency
  - Apply team upgrades (e.g., sharpness, armor tiers)
- Client UI is just a view; availability/eligibility is server-checked.

## Block limits and map protection
- Protect map template blocks from griefing.
  - Default: only player-placed blocks are breakable.
  - Optional allow-list: some template blocks are breakable.
- Place/break must check:
  - allowed block types
  - per-player/per-team placed-block limits
  - "can place" rules (no overlap, within bounds, not inside protected zones)

## Block interaction client-side (rendering only)
- Client tracks break progress locally for visual feedback (progress bar, overlay).
- Break progress overlay uses 10-stage textures (`destroy_stage_0..9.png`).
- Actual block removal happens only after server confirms via `BlockBroken` event.
- Client sends `TryBreakBlock` / `TryPlaceBlock` requests; server validates and responds.
- On `ActionRejected`, client resets break progress via `BlockInteraction::on_action_rejected()`.

## Match reset
- Prefer: destroy current match instance and recreate from the same map template.
- No persistent world state between sessions.

## Replication requirements
- Bed state, generator state, team upgrades must be replicated (snapshot or events).
- Every block change must be observable by clients (event + snapshot redundancy ok).

