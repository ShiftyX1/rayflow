# Migration plan (mandatory stages)

## Stage A — Boundary
- Create `shared/` and move shared types/enums/protocol there.
- Remove direct calls from client into game logic; replace with sending commands via transport.

Done when:
- Client builds without importing `server/`.

## Stage B — Server match loop
- Implement server loop: tick, command queue, snapshot/event output.
- Server owns `World/Rules`.

Current bridge (allowed during Stage B):
- Seed-synced procedural terrain may be used temporarily so server collisions match client rendering.
- Server must be the source of the seed (`ServerHello.worldSeed`).

Target direction (post-bridge):
- Replace procedural terrain with map templates/prefabs owned by the server.
- Client should ultimately render from replication (snapshots/events/chunk deltas), not generate authoritative terrain.

Done when:
- Basic actions (move, place/break blocks, buy) work through server even in one process (`LocalTransport`).

## Stage C — Client replica and cleanup
- Client contains no authoritative logic.
- Delete/disable old “bypass transport” code paths.

Done when:
- There is no place where client mutates world/inventory/damage directly.

## Stage D — Net readiness
- Transport interfaces stable.
- Protocol versioning + standard rejections.

Done when:
- `NetTransport` can be added without rewriting rules/world.

## Acceptance checklist
1. Offline match runs via `LocalTransport`.
2. Any world change happens only after server confirmation.
3. All game rules live in `server/` only.
4. Protocol has versioning and clear rejection reasons.
5. Logs exist on both sides (send/receive/apply).

## Prohibited moves
- No separate offline path with direct calls into server internals.
- No shared mutable state between server and client.
- No mixing render + rules + network in one class.
- No prediction/lag-compensation before Stage C completion.
