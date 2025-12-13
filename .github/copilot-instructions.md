
# Copilot system instructions for this project

This repository is a voxel-based BedWars-like game.

## Non-negotiable architecture rules
1. Authoritative server + thin client.
	- Server executes all simulation, rules, validation, match timing.
	- Client does rendering/UI/audio + captures input and sends commands.
2. Client never directly reads/writes server state.
3. One protocol for offline and online.
	- “Singleplayer” is the same server, connected via LocalTransport.
4. No special offline logic branch.
5. Do not add prediction/lag compensation until the refactor reaches Stage C.

## Mandatory code layering
The codebase must be physically separated into:
- shared/  (protocol/types/ids/enums/serialization)
- server/  (match/world/rules/validators/replication)
- client/  (render/ui/input/client replica)

Boundary: client/ must not depend on server/ (directly or indirectly). Only shared/.

## BedWars scope constraints
- No infinite procedural world, no persistent world saves.
- Maps are loaded from templates/prefabs (schematic-like).
- Match restart is implemented by re-instantiating the map template.

## Instruction files
When making changes, follow the detailed rules in:
- .github/instructions/architecture.instructions.md
- .github/instructions/protocol-transport.instructions.md
- .github/instructions/server-simulation.instructions.md
- .github/instructions/bedwars-gameplay.instructions.md
- .github/instructions/migration-plan.instructions.md

