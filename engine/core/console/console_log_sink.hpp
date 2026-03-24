#pragma once

// =============================================================================
// console_log_sink.hpp — Thread-safe ring buffer for routing engine logs to the
//                        in-game developer console.
// =============================================================================

#include "engine/core/export.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace engine::console {

struct LogEntry {
    double   timestamp{0.0};
    int      level{0};
    std::string message;
};

class RAYFLOW_CORE_API ConsoleLogSink {
public:
    static constexpr std::size_t kDefaultCapacity = 2048;

    explicit ConsoleLogSink(std::size_t capacity = kDefaultCapacity);

    // Push a formatted log line (thread-safe).
    void push(int level, const std::string& message);

    // Snapshot all entries into the caller's vector (thread-safe).
    // Avoids exposing internal storage across DLL boundary.
    void snapshot(std::vector<LogEntry>& out) const;

    // Clear all stored entries (thread-safe).
    void clear();

    // Enable / disable the sink.  When disabled, push() is a no-op.
    void set_enabled(bool v);
    bool enabled() const;

private:
    mutable std::mutex mu_;
    std::vector<LogEntry> buffer_;
    std::size_t head_{0};   // next write position
    std::size_t count_{0};  // entries currently stored
    std::size_t capacity_;
    bool enabled_{true};
};

} // namespace engine::console
