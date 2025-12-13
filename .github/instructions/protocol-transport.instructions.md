# Transport + Protocol (minimum spec)

## Transport contract
Client and server communicate only via an abstract transport.

### Required properties
- Two logical channels per connection:
  - client -> server
  - server -> client
- Per-connection ordering: **FIFO** is required.
- Messages are discrete (framed). Transport is not the protocol.

### Implementations
1. `LocalTransport` (single process): in-memory queues.
2. `NetTransport` (later): UDP/TCP/WebSocket are allowed, but the interface must stay stable.

Critical: client always “connects” to the transport abstraction; server always “listens” on it.

## Protocol versioning
- Every connection starts with version negotiation.
- Server may reject incompatible versions with a clear reason.

## Session flow (minimum)
1. `ClientHello { protocolVersion, clientName, settings }`
2. `ServerHello { acceptedVersion, tickRate, matchParams, mapId }`
3. `JoinMatch { requestedTeam? }`
4. `JoinAck { playerId, teamId, spawn, initialInventory }`

## Client -> Server commands (intent, not results)
Commands represent **player intent** and can be rejected by the server.

Minimum set:
- `InputFrame { seq, tick?, moveAxes, lookDelta, jump, sprint, crouch }`
- `TryPlaceBlock { seq, pos, blockType, face, hotbarSlot }`
- `TryBreakBlock { seq, pos }`
- `TryHit { seq, targetEntityId }`
- `TryBuy { seq, shopItemId }`
- `TryUseItem { seq, itemSlot, target? }`
- `TryOpenShop { seq, shopId }` (server validates availability; UI is still client-side)

## Server -> Client updates
Split into two kinds:

### 1) Snapshots (periodic)
- `StateSnapshot { serverTick, yourPlayerId, players[], entities[], beds[], generators[], inventoryDeltas?, blockDeltas? }`

Rules:
- Snapshots are periodic; they can be delta-compressed later.
- Client must be able to rebuild `ClientWorld` from snapshots + events.

### 2) Events (asap, may be batched)
- `BlockPlaced { pos, blockType, placedBy }`
- `BlockBroken { pos, brokenBy, drop? }`
- `PurchaseResult { seq, ok, reason?, grantedItems?, newBalance? }`
- `PlayerDied { playerId, reason, killerId? }`
- `PlayerRespawned { playerId, pos }`
- `BedDestroyed { teamId, destroyedBy }`
- `MatchEnded { winningTeamId }`
- `ChatMessage { from, text }`

## Rejections and errors
- `ActionRejected { seq, actionType, reason }`

Recommended rejection reasons (enum in shared):
- `OutOfRange`
- `NoLineOfSight`
- `InvalidPosition`
- `Collision`
- `ProtectedBlock`
- `NotEnoughResources`
- `Cooldown`
- `NotAllowed`
- `InvalidItem`
- `MatchNotRunning`

## Logging requirement
- Server logs: received command -> validated -> applied/rejected (+ reason).
- Client logs: sent command -> received snapshot/event -> applied to `ClientWorld`.
