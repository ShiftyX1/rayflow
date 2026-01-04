#pragma once

#include "messages.hpp"
#include <engine/core/byte_buffer.hpp>

#include <optional>
#include <vector>

namespace bedwars::proto {

// ============================================================================
// Serialization
// ============================================================================

/// Serialize a message to bytes.
std::vector<std::uint8_t> serialize(const Message& msg);

/// Deserialize bytes to a message.
/// Returns std::nullopt if parsing fails.
std::optional<Message> deserialize(std::span<const std::uint8_t> data);

} // namespace bedwars::proto
