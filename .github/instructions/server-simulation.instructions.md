# Server simulation rules

## Tick model
- Server runs a fixed tick loop. Use **30 ticks/sec** as the default (single authoritative timeline).
- All incoming commands are queued and applied on the next tick (or nearest tick).
- Server time is `serverTick` (integer). Clients are not a source of time.

Performance constraints (hard):
- Avoid allocations in the tick path.
- Avoid `std::string` building, `iostream`, and heavy formatting per tick.
- Prefer reuse: preallocated buffers, reserved vectors, fixed-size ring buffers.

## Determinism constraints
- Randomness (if any) must be server-only.
- Validation must not depend on client-side derived state.

Hard rule:
- Do not use global RNG (`std::rand`, `std::srand`) inside simulation. Use local PRNG state seeded by server-controlled values.

## Authoritative validation (required)
Every action must be validated on the server:
- Place block: placement rules, collision, distance, target face, support checks, map protection.
- Break block: distance/LOS, tool rules if any, protected blocks vs player-placed blocks.
- Hit: distance/LOS, attack cooldown, target validity.
- Buy/upgrade: currency check, shop availability, team upgrades.

## World ownership
- Server owns `World` and all rules.
- Server is responsible for generating replication outputs: snapshots + events.

## Anti-cheat baseline
- Never trust client-reported positions as truth.
- Movement is driven by `InputFrame` intent; server sim produces resulting position.

Current migration note:
- Gravity/jump and voxel collisions are already server-authoritative; the client must not run local player physics.

## Match lifecycle
- Match states: `Lobby -> Running -> Ending -> Reset` (exact naming may vary).
- Reset must fully restore arena by reloading the map template.
