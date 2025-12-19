# Testing Specification & Guidelines

## Overview

This document defines the comprehensive testing strategy for the rayflow voxel BedWars engine.
Tests are organized by layer (`shared/`, `server/`, `client/`) and by test type (unit, integration, system).

## Test Framework & Tooling

### Required dependencies
```cmake
# Add to CMakeLists.txt for test builds
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)
```

### Directory structure
```
tests/
├── shared/               # Tests for shared/ layer
│   ├── protocol/         # Message serialization, versioning
│   ├── transport/        # LocalTransport, endpoint contract
│   ├── voxel/            # Block types, light props, util functions
│   └── maps/             # RFMAP I/O, round-trip tests
│
├── server/               # Tests for server/ layer
│   ├── core/             # Server tick loop, session flow
│   ├── voxel/            # Terrain generation, collision
│   ├── validation/       # Action validators (place/break/hit)
│   └── simulation/       # Physics, movement, determinism
│
├── client/               # Tests for client/ layer (render-less)
│   ├── ecs/              # Component systems (mocked rendering)
│   ├── voxel/            # ClientWorld state, chunk meshing logic
│   └── net/              # ClientSession message handling
│
├── integration/          # Cross-layer integration tests
│   ├── session/          # Full client-server session via LocalTransport
│   ├── protocol/         # Protocol version negotiation
│   └── replication/      # Snapshot/event consistency
│
├── fixtures/             # Test data (maps, configs, mock inputs)
│   ├── maps/             # Small .rfmap files for testing
│   └── inputs/           # Recorded InputFrame sequences
│
└── helpers/              # Test utilities and mocks
    ├── mock_endpoint.hpp # Mock transport endpoint
    ├── mock_terrain.hpp  # Deterministic terrain for testing
    └── test_utils.hpp    # Common test utilities
```

## Hard Rules for Tests

### 1. Layer isolation (critical)
- `tests/server/` must **never** include raylib headers or client code
- `tests/client/` must **never** include server internals
- Both can include `shared/` freely
- Integration tests may include both, but through proper transport abstraction

### 2. Determinism (critical)
- All tests must be **deterministic** and **reproducible**
- No use of `std::rand()`, `std::srand()`, or system time in test logic
- Use fixed seeds for any PRNG-based tests
- Tests must produce identical results on every run

### 3. No external dependencies
- Tests must not require network connectivity
- Tests must not depend on filesystem state outside `tests/fixtures/`
- Tests must not depend on specific system clock values

### 4. Performance tests are separate
- Performance/benchmark tests go in `tests/benchmarks/`
- Performance tests must have clear pass/fail thresholds documented
- Never mix functional tests with performance tests

## Test Categories

### Unit Tests

#### Shared layer (`tests/shared/`)

**Protocol tests (`tests/shared/protocol/`)**
```cpp
// test_messages.cpp
TEST_CASE("Message variant construction") {
    // All message types can be constructed with defaults
    // All message types can be stored in Message variant
}

TEST_CASE("RejectReason enum stability") {
    // Enum values must not change (binary compatibility)
    REQUIRE(static_cast<int>(RejectReason::Unknown) == 0);
    REQUIRE(static_cast<int>(RejectReason::Invalid) == 1);
    // ... all values
}

TEST_CASE("Protocol version constant") {
    REQUIRE(kProtocolVersion >= 1);
}
```

**Transport tests (`tests/shared/transport/`)**
```cpp
// test_local_transport.cpp
TEST_CASE("LocalTransport pair creation") {
    auto pair = LocalTransport::create_pair();
    REQUIRE(pair.client != nullptr);
    REQUIRE(pair.server != nullptr);
}

TEST_CASE("LocalTransport FIFO ordering") {
    // Messages sent in order A, B, C must be received in order A, B, C
}

TEST_CASE("LocalTransport bidirectional") {
    // Client->Server and Server->Client are independent channels
}

TEST_CASE("LocalTransport try_recv returns false when empty") {
    // No message available = false, does not block
}
```

**Voxel tests (`tests/shared/voxel/`)**
```cpp
// test_block.cpp
TEST_CASE("BlockType enum stability") {
    // BlockType values must not change (save compatibility)
    REQUIRE(static_cast<int>(BlockType::Air) == 0);
    REQUIRE(static_cast<int>(BlockType::Stone) == 1);
    // ... all types
}

TEST_CASE("BlockType::Count is correct") {
    REQUIRE(static_cast<int>(BlockType::Count) == 16);
}

TEST_CASE("Block light props array size matches Count") {
    constexpr auto size = sizeof(BLOCK_LIGHT_PROPS) / sizeof(BLOCK_LIGHT_PROPS[0]);
    REQUIRE(size == static_cast<std::size_t>(BlockType::Count));
}

TEST_CASE("is_solid utility") {
    REQUIRE_FALSE(util::is_solid(BlockType::Air));
    REQUIRE_FALSE(util::is_solid(BlockType::Water));
    REQUIRE(util::is_solid(BlockType::Stone));
    REQUIRE(util::is_solid(BlockType::Bedrock));
}

TEST_CASE("is_transparent utility") {
    REQUIRE(util::is_transparent(BlockType::Air));
    REQUIRE(util::is_transparent(BlockType::Water));
    REQUIRE(util::is_transparent(BlockType::Leaves));
    REQUIRE_FALSE(util::is_transparent(BlockType::Stone));
}

TEST_CASE("Chunk dimension constants") {
    REQUIRE(CHUNK_WIDTH == 16);
    REQUIRE(CHUNK_HEIGHT == 256);
    REQUIRE(CHUNK_DEPTH == 16);
    REQUIRE(CHUNK_SIZE == CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH);
}
```

**Maps I/O tests (`tests/shared/maps/`)**
```cpp
// test_rfmap_io.cpp
TEST_CASE("RFMAP save and load round-trip") {
    // Create template, save, load, compare
}

TEST_CASE("RFMAP handles empty map") {
    // Edge case: 0x0x0 bounds
}

TEST_CASE("RFMAP handles max bounds") {
    // Stress test: large map template
}

TEST_CASE("RFMAP invalid file returns error") {
    // Corrupt/missing file handling
}
```

#### Server layer (`tests/server/`)

**Core server tests (`tests/server/core/`)**
```cpp
// test_server_lifecycle.cpp
TEST_CASE("Server starts and stops cleanly") {
    auto pair = LocalTransport::create_pair();
    Server server(pair.server);
    server.start();
    server.stop();
    // No crash, no leak, thread joined
}

TEST_CASE("Server rejects connection without ClientHello") {
    // Send non-hello message first, expect disconnect or rejection
}

TEST_CASE("Server protocol version mismatch") {
    // ClientHello with wrong version -> rejected
}
```

**Session flow tests (`tests/server/core/`)**
```cpp
// test_session_flow.cpp
TEST_CASE("Full handshake: Hello -> ServerHello -> JoinMatch -> JoinAck") {
    // Happy path session establishment
}

TEST_CASE("Server sends StateSnapshot periodically") {
    // After join, client receives snapshots at tick rate
}
```

**Validation tests (`tests/server/validation/`)**
```cpp
// test_block_validation.cpp
TEST_CASE("TryPlaceBlock: valid placement accepted") {
    // Empty air position, player has block, within range -> BlockPlaced
}

TEST_CASE("TryPlaceBlock: collision rejected") {
    // Position already occupied -> ActionRejected(Collision)
}

TEST_CASE("TryPlaceBlock: out of range rejected") {
    // Position too far from player -> ActionRejected(OutOfRange)
}

TEST_CASE("TryPlaceBlock: protected block rejected") {
    // Trying to place in protected zone -> ActionRejected(ProtectedBlock)
}

TEST_CASE("TryBreakBlock: valid break accepted") {
    // Breakable block, within range -> BlockBroken
}

TEST_CASE("TryBreakBlock: protected block rejected") {
    // Template block / bedrock -> ActionRejected(ProtectedBlock)
}

TEST_CASE("TryBreakBlock: air rejected") {
    // Breaking air -> ActionRejected(Invalid)
}
```

**Physics/simulation tests (`tests/server/simulation/`)**
```cpp
// test_physics.cpp
TEST_CASE("Gravity applies when not on ground") {
    // Player in air -> vy decreases each tick
}

TEST_CASE("Ground collision stops fall") {
    // Player falling onto solid block -> onGround=true, vy=0
}

TEST_CASE("Jump only when on ground") {
    // Jump input while on ground -> vy impulse
    // Jump input while in air -> no effect
}

TEST_CASE("Movement input produces velocity") {
    // InputFrame with moveX/moveY -> position changes
}

TEST_CASE("Collision with walls") {
    // Movement blocked by solid blocks
}

// test_determinism.cpp
TEST_CASE("Same inputs produce same outputs") {
    // Two server instances with same seed + same input sequence
    // -> identical final state
}

TEST_CASE("Tick order is deterministic") {
    // Process N ticks, record state at each
    // Repeat, verify identical
}
```

**Terrain tests (`tests/server/voxel/`)**
```cpp
// test_terrain.cpp
TEST_CASE("Terrain generation is deterministic") {
    // Same seed -> identical terrain
}

TEST_CASE("Terrain get_block returns correct type") {
    // Known seed + known position -> expected block type
}

TEST_CASE("Terrain set_block persists") {
    // set_block then get_block -> same value
}

TEST_CASE("Terrain out of bounds returns Air") {
    // Negative coords or beyond limits -> Air
}
```

#### Client layer (`tests/client/`)

**Note:** Client tests must not use raylib functions. Mock rendering interfaces.

**ClientSession tests (`tests/client/net/`)**
```cpp
// test_client_session.cpp
TEST_CASE("ClientSession connects and sends ClientHello") {
    // On connect, automatically sends ClientHello
}

TEST_CASE("ClientSession receives ServerHello") {
    // ServerHello is parsed and stored
}

TEST_CASE("ClientSession applies StateSnapshot") {
    // StateSnapshot updates local player position
}

TEST_CASE("ClientSession handles BlockPlaced event") {
    // BlockPlaced -> updates ClientWorld
}

TEST_CASE("ClientSession handles BlockBroken event") {
    // BlockBroken -> removes block from ClientWorld
}

TEST_CASE("ClientSession handles ActionRejected") {
    // ActionRejected -> can be queried or triggers callback
}
```

**ClientWorld tests (`tests/client/voxel/`)**
```cpp
// test_client_world.cpp
TEST_CASE("ClientWorld stores and retrieves blocks") {
    // set_block + get_block round-trip
}

TEST_CASE("ClientWorld handles chunk boundaries") {
    // Blocks at chunk edges are stored correctly
}
```

### Integration Tests (`tests/integration/`)

**Session integration (`tests/integration/session/`)**
```cpp
// test_full_session.cpp
TEST_CASE("Full game session via LocalTransport") {
    // 1. Create LocalTransport pair
    // 2. Start server
    // 3. Create client session
    // 4. Complete handshake
    // 5. Send InputFrames
    // 6. Receive StateSnapshots
    // 7. Verify position updates
    // 8. Clean shutdown
}

TEST_CASE("Block placement end-to-end") {
    // 1. Join session
    // 2. Client sends TryPlaceBlock
    // 3. Server validates and broadcasts BlockPlaced
    // 4. Client receives and applies to ClientWorld
    // 5. Verify block exists in ClientWorld
}

TEST_CASE("Block break with rejection") {
    // 1. Join session
    // 2. Client sends TryBreakBlock for protected block
    // 3. Server sends ActionRejected
    // 4. Client receives rejection
    // 5. Verify block still exists
}
```

**Replication tests (`tests/integration/replication/`)**
```cpp
// test_replication.cpp
TEST_CASE("Client world matches server after N ticks") {
    // Run session for N ticks
    // Compare client's view of world with server's authoritative state
}

TEST_CASE("Late join receives current state") {
    // Server runs, places blocks, then client joins
    // Client receives snapshot with current world state
}
```

## Mocking Strategy

### Mock Endpoint
```cpp
// tests/helpers/mock_endpoint.hpp
class MockEndpoint : public shared::transport::IEndpoint {
public:
    void send(shared::proto::Message msg) override {
        sent_messages_.push_back(std::move(msg));
    }

    bool try_recv(shared::proto::Message& outMsg) override {
        if (incoming_messages_.empty()) return false;
        outMsg = std::move(incoming_messages_.front());
        incoming_messages_.pop();
        return true;
    }

    // Test helpers
    void inject_message(shared::proto::Message msg);
    const std::vector<shared::proto::Message>& sent() const;
    void clear();

private:
    std::vector<shared::proto::Message> sent_messages_;
    std::queue<shared::proto::Message> incoming_messages_;
};
```

### Mock Terrain (deterministic)
```cpp
// tests/helpers/mock_terrain.hpp
class MockTerrain {
public:
    // Fixed pattern: solid floor at y=64, air above
    BlockType get_block(int x, int y, int z) const {
        if (y < 64) return BlockType::Stone;
        return BlockType::Air;
    }
};
```

## Test Naming Convention

```
test_<module>_<feature>.cpp        // File name
<Module>_<Feature>_<Scenario>      // Test case name (optional: BDD style)
```

Examples:
- `test_local_transport_fifo.cpp`
- `TEST_CASE("LocalTransport_Send_MaintainsFIFOOrder")`

Or BDD style:
- `SCENARIO("LocalTransport maintains FIFO ordering")`
- `GIVEN("a LocalTransport pair")`, `WHEN("messages are sent")`, `THEN("received in order")`

## Coverage Requirements

### Minimum coverage targets
| Layer    | Line Coverage | Branch Coverage |
|----------|---------------|-----------------|
| shared/  | 90%           | 80%             |
| server/  | 85%           | 75%             |
| client/  | 70%           | 60%             |

### Critical paths (must be 100% covered)
- Protocol message construction and variant handling
- Transport FIFO ordering
- Server validation (all RejectReason paths)
- Determinism-sensitive code (physics tick, RNG seeding)
- Block type enum and utility functions

## Running Tests

### CMake configuration
```cmake
# Enable testing
option(RAYFLOW_BUILD_TESTS "Build tests" OFF)

if (RAYFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### Test executable targets
```cmake
# tests/CMakeLists.txt
add_executable(rayflow_tests
    shared/protocol/test_messages.cpp
    shared/transport/test_local_transport.cpp
    shared/voxel/test_block.cpp
    # ... all test files
)

target_link_libraries(rayflow_tests PRIVATE
    shared_lib
    server_lib
    Catch2::Catch2WithMain
)

# Discover and register tests
include(CTest)
include(Catch)
catch_discover_tests(rayflow_tests)
```

### Running
```bash
# Build with tests
cmake -B build -DRAYFLOW_BUILD_TESTS=ON
cmake --build build

# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./build/rayflow_tests "[LocalTransport]"

# Run with verbose output
./build/rayflow_tests -v high
```

## Writing New Tests

### Checklist for new test
- [ ] Follows layer isolation rules
- [ ] Is deterministic (no randomness, no time dependency)
- [ ] Has clear test name describing scenario
- [ ] Tests one logical concept per TEST_CASE
- [ ] Uses appropriate section/subcases for variations
- [ ] Includes edge cases (empty, max, boundary)
- [ ] Documents any non-obvious setup or assertions

### Template for new test file
```cpp
#include <catch2/catch_test_macros.hpp>

// Include only what's needed from the layer being tested
#include "shared/voxel/block.hpp"

using namespace shared::voxel;

TEST_CASE("FeatureName_Scenario_ExpectedBehavior", "[tag1][tag2]") {
    // Arrange
    // ...

    // Act
    // ...

    // Assert
    REQUIRE(/* condition */);
}
```

## Continuous Integration

### Required CI checks
1. All tests pass (`ctest --output-on-failure`)
2. No new compiler warnings (`-Wall -Wextra -Werror`)
3. Coverage does not decrease (fail if coverage drops)
4. Static analysis passes (clang-tidy)

### CI test matrix
- macOS (arm64) + Clang
- Linux (x64) + GCC
- Windows (x64) + MSVC

## Anti-patterns to Avoid

### ❌ Don't
- Test implementation details (private methods, internal state)
- Use sleep/delays for synchronization
- Depend on test execution order
- Share mutable state between tests
- Use real filesystem for unit tests (use fixtures or mocks)
- Include raylib in server tests
- Use `std::rand()` in tests

### ✅ Do
- Test public interfaces and contracts
- Use proper synchronization primitives
- Make each test independent
- Use fresh state for each test
- Use in-memory test data or fixtures
- Mock rendering for client tests
- Use fixed seeds for reproducible tests

## Glossary

| Term | Definition |
|------|------------|
| Unit test | Tests a single function/class in isolation |
| Integration test | Tests multiple components working together |
| System test | Tests the full system end-to-end |
| Fixture | Pre-defined test data (maps, configs, input sequences) |
| Mock | Fake implementation of a dependency |
| Deterministic | Produces identical output for identical input |
| FIFO | First-In-First-Out ordering |
| Coverage | Percentage of code exercised by tests |

