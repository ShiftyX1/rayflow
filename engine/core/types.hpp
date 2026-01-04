#pragma once

#include <cstdint>
#include <string>

namespace engine {

// ============================================================================
// Core Types
// ============================================================================

using PlayerId = std::uint32_t;
using Tick = std::uint64_t;

static constexpr PlayerId kInvalidPlayerId = 0;

// ============================================================================
// Logging
// ============================================================================

enum class LogLevel : std::uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
};

// ============================================================================
// Connection State
// ============================================================================

enum class ConnectionState : std::uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
};

} // namespace engine
