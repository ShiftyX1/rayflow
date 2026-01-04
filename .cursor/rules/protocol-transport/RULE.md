# Transport + Protocol (minimum spec)

## Transport contract
Client and server communicate only via an abstract transport.

### Required properties
- Two logical channels per connection:
  - client → server
  - server → client
- Per-connection ordering: **FIFO** is required.
- Messages are discrete (framed). Transport is not the protocol.

### Implementations
1. `LocalTransport` (single process): in-memory queues.
2. `ENetTransport` ✅ **IMPLEMENTED**: UDP-based reliable transport using ENet library.

#### Transport Files
```
engine/transport/
├── client_transport.hpp       # IClientTransport interface
├── server_transport.hpp       # IServerTransport interface
├── local_transport.hpp/cpp    # In-memory (singleplayer)
├── enet_server.hpp/cpp        # Multi-client server listener
└── enet_client.hpp/cpp        # Client connector
```

#### ENet Channels
```cpp
enum class ENetChannel : std::uint8_t {
    Reliable = 0,        // Hello, Join, Events, Chat
    Unreliable = 1,      // StateSnapshot, InputFrame
    ReliableOrdered = 2, // Sequenced reliable data
    Count = 3
};
```

#### Usage
- Server: `ENetServer::start(port, maxClients)` → `poll(0)` every tick
- Client: `ENetClient::connect(host, port)` → `poll(0)` every frame
- Both use `ENetConnection` as `IEndpoint` for send/recv

Critical: client always "connects" to the transport abstraction; server always "listens" on it.

## Protocol versioning
- Every connection starts with version negotiation.
- Server may reject incompatible versions with a clear reason.

## Session flow (minimum)
1. `ClientHello { protocolVersion, clientName, settings }`
2. `ServerHello { acceptedVersion, tickRate, worldSeed?, matchParams, mapId }`
3. `JoinMatch { requestedTeam? }`
4. `JoinAck { playerId, teamId, spawn, initialInventory }`

Note (current migration state):
- `worldSeed` is currently used so the client can render the same temporary terrain that the server collides against.
- This is a temporary bridge; the target BedWars architecture is server-owned map templates + replication (snapshots/events/chunk deltas), not client-side procedural generation.

## Client → Server commands (intent, not results)
Commands represent **player intent** and can be rejected by the server.

Minimum set:
- `InputFrame { seq, tick?, moveAxes, lookDelta, jump, sprint, crouch }`
- `TryPlaceBlock { seq, pos, blockType, face, hotbarSlot }`
- `TryBreakBlock { seq, pos }`
- `TryHit { seq, targetEntityId }`
- `TryBuy { seq, shopItemId }`
- `TryUseItem { seq, itemSlot, target? }`
- `TryOpenShop { seq, shopId }` (server validates availability; UI is still client-side)

## Server → Client updates
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
- Server logs: received command → validated → applied/rejected (+ reason).
- Client logs: sent command → received snapshot/event → applied to `ClientWorld`.

