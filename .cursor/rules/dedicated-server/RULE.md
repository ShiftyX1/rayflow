# Dedicated Server (RFDS) Instructions

## Overview

**RFDS** (RayFlow Dedicated Server) is a headless server executable for hosting multiplayer matches. It runs the same `Server` logic as singleplayer but uses network transport instead of `LocalTransport`.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        rayflow.exe (Client)                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │  Renderer   │  │  UI/Input   │  │  ClientSession          │ │
│  └─────────────┘  └─────────────┘  │  ┌───────────────────┐  │ │
│                                     │  │ IEndpoint (Net)   │──┼─┼──► Network
│                                     │  └───────────────────┘  │ │
│                                     └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                        rfds.exe (Dedicated Server)              │
│  ┌─────────────────┐  ┌───────────────────────────────────────┐ │
│  │  Server         │  │  NetTransport                         │ │
│  │  ┌───────────┐  │  │  ┌─────────────────────────────────┐  │ │
│  │  │ Match     │  │  │  │ ConnectionManager               │  │ │
│  │  │ Terrain   │  │  │  │ - accept_connections()          │  │ │
│  │  │ Scripts   │  │  │  │ - per-client IEndpoint          │  │ │
│  │  └───────────┘  │  │  └─────────────────────────────────┘  │ │
│  └────────┬────────┘  └──────────────────┬────────────────────┘ │
│           │                              │                      │
│           └──────────────────────────────┘                      │
│                      tick loop                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Current State

### Implemented ✅
- [x] `IEndpoint` interface (`shared/transport/endpoint.hpp`)
- [x] `LocalTransport` for singleplayer (`shared/transport/local_transport.hpp`)
- [x] `Server` class with tick loop (`server/core/server.hpp`)
- [x] Protocol messages (`shared/protocol/messages.hpp`)
- [x] `ScriptEngine` for map scripts (`server/scripting/`)

### Not Implemented 🔴
- [ ] `NetTransport` (UDP/TCP network layer)
- [ ] `ConnectionManager` (multiple clients)
- [ ] `rfds.exe` entry point
- [ ] RCON (remote console)
- [ ] Server browser integration

---

## Implementation Roadmap

### Phase 1: Network Transport (2-3 weeks)

**Goal**: Replace `LocalTransport` with network-capable transport.

#### File Structure
```
shared/transport/
├── endpoint.hpp              # IEndpoint interface (exists)
├── local_transport.hpp/cpp   # In-memory queues (exists)
├── enet_transport.hpp/cpp    # ENet-based transport [NEW]
├── enet_server.hpp/cpp       # Server connection manager [NEW]
├── enet_client.hpp/cpp       # Client connector [NEW]
└── enet_common.hpp           # Shared ENet utilities [NEW]

app/
├── main.cpp                  # Client entry (exists)
├── dedicated_server_main.cpp # RFDS entry [NEW]
└── ...
```

#### Why ENet

| Feature | ENet | Raw UDP | TCP |
|---------|------|---------|-----|
| Reliable delivery | ✅ Built-in | ❌ Manual | ✅ Built-in |
| Ordered delivery | ✅ Optional per-channel | ❌ Manual | ✅ Always |
| No head-of-line blocking | ✅ | ✅ | ❌ |
| Low latency | ✅ | ✅ | ❌ |
| Fragmentation | ✅ Built-in | ❌ Manual | ✅ Built-in |
| Connection management | ✅ Built-in | ❌ Manual | ✅ Built-in |
| Complexity | Medium | High | Low |

**ENet** — лучший баланс между производительностью и простотой для игрового сервера.

#### ENet Channels

```cpp
// Channel allocation for message types
enum class ENetChannel : uint8_t {
    Reliable = 0,      // Important: connects, disconnects, game events
    Unreliable = 1,    // Snapshots, position updates (can be dropped)
    ReliableOrdered = 2, // Chat, commands (must be in order)
    
    Count = 3
};
```

#### ENet Transport Design

```cpp
// shared/transport/enet_common.hpp

#include <enet/enet.h>
#include <memory>
#include <string>

namespace shared::transport {

// RAII wrapper for ENet initialization
class ENetInitializer {
public:
    ENetInitializer() { enet_initialize(); }
    ~ENetInitializer() { enet_deinitialize(); }
    
    ENetInitializer(const ENetInitializer&) = delete;
    ENetInitializer& operator=(const ENetInitializer&) = delete;
};

// Message serialization helpers
std::vector<uint8_t> serialize_message(const proto::Message& msg);
bool deserialize_message(const uint8_t* data, size_t size, proto::Message& outMsg);

// Packet flags based on message type
uint32_t get_packet_flags(proto::MessageType type);
uint8_t get_channel(proto::MessageType type);

} // namespace shared::transport
```

```cpp
// shared/transport/enet_transport.hpp

namespace shared::transport {

// Single peer connection (implements IEndpoint)
class ENetConnection : public IEndpoint {
public:
    explicit ENetConnection(ENetPeer* peer);
    ~ENetConnection();
    
    // IEndpoint interface
    void send(proto::Message msg) override;
    bool try_recv(proto::Message& outMsg) override;
    
    // Connection state
    bool is_connected() const;
    void disconnect();
    void disconnect_now();  // Immediate, no graceful shutdown
    
    // Stats
    uint32_t ping_ms() const;
    uint32_t packet_loss() const;  // Percentage * 100
    uint64_t bytes_sent() const;
    uint64_t bytes_recv() const;
    
    // Internal
    ENetPeer* peer() const { return peer_; }
    void queue_received(proto::Message msg);
    
private:
    ENetPeer* peer_{nullptr};
    std::queue<proto::Message> recvQueue_;
    std::mutex mutex_;
    
    uint64_t bytesSent_{0};
    uint64_t bytesRecv_{0};
};

} // namespace shared::transport
```

```cpp
// shared/transport/enet_server.hpp

namespace shared::transport {

class ENetServer {
public:
    ENetServer();
    ~ENetServer();
    
    // Lifecycle
    bool start(uint16_t port, size_t maxClients = 32);
    void stop();
    bool is_running() const;
    
    // Must be called every tick
    void poll(uint32_t timeoutMs = 0);
    
    // Broadcast to all connected clients
    void broadcast(const proto::Message& msg);
    
    // Access connections
    const std::vector<std::shared_ptr<ENetConnection>>& connections() const;
    std::shared_ptr<ENetConnection> find_connection(ENetPeer* peer);
    
    // Callbacks
    std::function<void(std::shared_ptr<ENetConnection>)> onConnect;
    std::function<void(std::shared_ptr<ENetConnection>)> onDisconnect;
    
private:
    void handle_connect(ENetEvent& event);
    void handle_disconnect(ENetEvent& event);
    void handle_receive(ENetEvent& event);
    
    ENetHost* host_{nullptr};
    std::vector<std::shared_ptr<ENetConnection>> connections_;
    bool running_{false};
};

} // namespace shared::transport
```

```cpp
// shared/transport/enet_client.hpp

namespace shared::transport {

class ENetClient {
public:
    ENetClient();
    ~ENetClient();
    
    // Connection
    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000);
    void disconnect();
    bool is_connected() const;
    
    // Must be called every frame
    void poll(uint32_t timeoutMs = 0);
    
    // Get endpoint for ClientSession
    std::shared_ptr<ENetConnection> connection() { return connection_; }
    
    // Callbacks
    std::function<void()> onConnect;
    std::function<void()> onDisconnect;
    
private:
    ENetHost* host_{nullptr};
    std::shared_ptr<ENetConnection> connection_;
    bool connected_{false};
};

} // namespace shared::transport
```

#### Implementation Details

```cpp
// shared/transport/enet_server.cpp (key parts)

bool ENetServer::start(uint16_t port, size_t maxClients) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;
    
    host_ = enet_host_create(
        &address,
        maxClients,
        static_cast<size_t>(ENetChannel::Count),
        0,  // Unlimited incoming bandwidth
        0   // Unlimited outgoing bandwidth
    );
    
    if (!host_) {
        return false;
    }
    
    // Enable compression
    enet_host_compress_with_range_coder(host_);
    
    running_ = true;
    return true;
}

void ENetServer::poll(uint32_t timeoutMs) {
    if (!host_) return;
    
    ENetEvent event;
    while (enet_host_service(host_, &event, timeoutMs) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                handle_connect(event);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                handle_disconnect(event);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                handle_receive(event);
                enet_packet_destroy(event.packet);
                break;
            default:
                break;
        }
        timeoutMs = 0;  // Don't wait after first event
    }
}

void ENetServer::broadcast(const proto::Message& msg) {
    auto data = serialize_message(msg);
    auto flags = get_packet_flags(msg.type);
    auto channel = get_channel(msg.type);
    
    ENetPacket* packet = enet_packet_create(
        data.data(), data.size(), flags
    );
    
    enet_host_broadcast(host_, channel, packet);
}
```

### Phase 2: Dedicated Server Executable (1-2 weeks)

**Goal**: `rfds.exe` can host matches.

#### Entry Point

```cpp
// app/dedicated_server_main.cpp

#include "server/core/server.hpp"
#include "shared/transport/enet_server.hpp"
#include "shared/transport/enet_common.hpp"

#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

namespace {
    volatile std::sig_atomic_t g_running = 1;
    
    void signal_handler(int) {
        g_running = 0;
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Parse args: --port, --map, --config, etc.
    RFDSConfig config = parse_args(argc, argv);
    
    // Initialize ENet (RAII)
    shared::transport::ENetInitializer enetInit;
    
    // Create network server
    shared::transport::ENetServer netServer;
    if (!netServer.start(config.port, config.maxPlayers)) {
        std::cerr << "Failed to start on port " << config.port << std::endl;
        return 1;
    }
    
    std::cout << "RFDS started on port " << config.port << std::endl;
    
    // Create game server
    server::Server gameServer;
    gameServer.init(config.tickRate);
    gameServer.load_map(config.mapPath);
    
    // Wire up connections
    netServer.onConnect = [&](auto conn) {
        std::cout << "Client connected (ping: " << conn->ping_ms() << "ms)" << std::endl;
        gameServer.add_client(conn);
    };
    netServer.onDisconnect = [&](auto conn) {
        std::cout << "Client disconnected" << std::endl;
        gameServer.remove_client(conn);
    };
    
    // Tick timing
    const auto tickDuration = std::chrono::microseconds(1'000'000 / config.tickRate);
    auto nextTick = std::chrono::steady_clock::now();
    
    // Main loop
    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        
        // Process network events (non-blocking)
        netServer.poll(0);
        
        // Run game tick
        gameServer.tick();
        
        // Sleep until next tick
        nextTick += tickDuration;
        if (nextTick > now) {
            std::this_thread::sleep_until(nextTick);
        } else {
            // We're behind, skip sleep but don't accumulate debt
            nextTick = now;
        }
    }
    
    std::cout << "Shutting down..." << std::endl;
    netServer.stop();
    return 0;
}
```
    }
    
    netServer.stop();
    return 0;
}
```

#### Server Multi-Client Support

```cpp
// server/core/server.hpp additions

class Server {
public:
    // Existing
    void init(int tickRate);
    void tick();
    
    // New for multi-client
    void add_client(std::shared_ptr<IEndpoint> endpoint);
    void remove_client(std::shared_ptr<IEndpoint> endpoint);
    
    // Script integration
    void set_script_engine(ScriptEngine* engine);
    
private:
    struct ClientConnection {
        std::shared_ptr<IEndpoint> endpoint;
        uint32_t playerId;
        SessionState state;  // Connecting, InGame, Disconnecting
    };
    
    std::vector<ClientConnection> clients_;
    
    void process_client_messages();
    void broadcast(const proto::Message& msg);
    void send_to(uint32_t playerId, const proto::Message& msg);
};
```

### Phase 3: Script Integration (1 week)

**Goal**: Scripts work identically in singleplayer and dedicated server.

#### Script Events from Network

```cpp
// In Server::process_client_messages()

void Server::process_client_messages() {
    for (auto& client : clients_) {
        proto::Message msg;
        while (client.endpoint->try_recv(msg)) {
            // Process message...
            
            // Notify scripts
            if (msg.type == proto::MessageType::PlayerJoined) {
                scriptEngine_->on_player_join(client.playerId);
            }
        }
    }
}

// Script commands become network messages
void Server::execute_script_commands() {
    auto commands = scriptEngine_->take_commands();
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case ScriptCommand::Type::Broadcast:
                broadcast(proto::ChatMessage{cmd.stringParam});
                break;
            case ScriptCommand::Type::SendToPlayer:
                send_to(cmd.playerIdParam, proto::ChatMessage{cmd.stringParam});
                break;
            // ...
        }
    }
}
```

### Phase 4: Production Features (When Needed)

| Feature | Priority | Description |
|---------|----------|-------------|
| RCON | 🟡 High | Remote admin console (TCP-based) |
| Server browser | 🟢 Normal | Heartbeat to master server |
| Match recording | 🟢 Normal | Demo files for replays |
| Anti-cheat hooks | 🟢 Normal | Server-side validation logging |
| Load balancing | 🔵 Low | Multiple matches per process |

---

## Configuration

### Command Line Arguments

```bash
rfds.exe [options]

Options:
  --port <port>       Listen port (default: 7777)
  --map <path>        Map file to load (.rfmap)
  --config <path>     Server config file (.conf)
  --tickrate <n>      Server tick rate (default: 30)
  --maxplayers <n>    Maximum players (default: 16)
  --rcon-port <port>  RCON port (default: disabled)
  --rcon-pass <pass>  RCON password
  --verbose           Verbose logging
```

### Config File (rfds.conf)

```ini
[server]
port = 7777
tickrate = 30
max_players = 16
map = maps/bedwars_classic.rfmap

[rcon]
enabled = false
port = 7778
password = 

[scripts]
engine_scripts = scripts/
reload_on_change = false  # Debug only

[logging]
level = info  # debug, info, warn, error
file = logs/rfds.log
```

---

## Hard Rules

### Networking (ENet-specific)

1. **Same protocol for Local and ENet** — `IEndpoint` interface must be identical.
2. **Use channels correctly**:
   - Channel 0 (Reliable): Important events, chat, commands
   - Channel 1 (Unreliable): Position updates, snapshots
   - Channel 2 (ReliableOrdered): Sequenced reliable data
3. **Server never trusts client** — All validation on server side.
4. **Graceful disconnect handling** — Use `enet_peer_disconnect()`, not `disconnect_now()`.
5. **Call `poll()` every tick** — ENet needs regular servicing.
6. **Destroy packets after receive** — `enet_packet_destroy(event.packet)`.

### Multi-Client

1. **Thread-safe message queues** — If using threads for network.
2. **Per-client state isolation** — One client's error doesn't crash others.
3. **Use `enet_host_broadcast()`** — Don't loop and send individually.

### Scripts

1. **Same API for singleplayer and dedicated** — Scripts don't know the difference.
2. **Script errors don't crash server** — Catch and log, continue.
3. **Script commands are queued** — Executed in batch after all scripts run.

### Performance

1. **Tick budget** — At 30 TPS, each tick has ~33ms budget.
2. **Enable compression** — `enet_host_compress_with_range_coder()`.
3. **Delta compression** — Only send changed state (future optimization).
4. **Bandwidth limits** — Use `enet_host_bandwidth_limit()` if needed.

---

## Testing

### Unit Tests
- [ ] ENet packet serialization/deserialization
- [ ] Connection state machine
- [ ] Multi-client message routing
- [ ] Channel selection logic

### Integration Tests
- [ ] Client connects to RFDS via ENet
- [ ] Multiple clients in same match
- [ ] Client disconnect handling (graceful + timeout)
- [ ] Script events fire correctly over network
- [ ] Reconnection after disconnect

### Load Tests
- [ ] 16 players sustained
- [ ] Simulated packet loss (ENet handles this)
- [ ] Simulated latency
- [ ] Bandwidth stress test

---

## CMake Integration

```cmake
# CMakeLists.txt (root) - Add ENet via vcpkg
find_package(unofficial-enet CONFIG REQUIRED)

# shared/CMakeLists.txt
target_link_libraries(rayflow_shared PUBLIC unofficial::enet::enet)

# app/CMakeLists.txt additions

# Dedicated server executable
add_executable(rfds
    dedicated_server_main.cpp
)

target_link_libraries(rfds PRIVATE
    rayflow_server
    rayflow_shared
    unofficial::enet::enet
    # No raylib! Server is headless
)

# Windows: ws2_32 for sockets (ENet needs this)
if(WIN32)
    target_link_libraries(rfds PRIVATE ws2_32 winmm)
endif()
```

### vcpkg Installation

```bash
# Install ENet via vcpkg
vcpkg install enet:x64-windows-static

# Or add to vcpkg.json
{
  "dependencies": [
    "enet"
  ]
}
```

---

## References

- **ENet documentation**: http://enet.bespin.org/
- **ENet tutorial**: http://enet.bespin.org/Tutorial.html
- **Valve Source Dedicated Server**: https://developer.valvesoftware.com/wiki/Source_Dedicated_Server
- **Gaffer On Games (Networking)**: https://gafferongames.com/
