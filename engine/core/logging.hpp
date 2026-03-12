#pragma once

// =============================================================================
// logging.hpp — Logging facade (replaces raylib TraceLog / LOG_* constants)
//
// Phase 0 migration: provides log level constants and a simple TraceLog
// implementation that writes to stderr. Phase 1+ may replace with a more
// sophisticated system.
// =============================================================================

#include <chrono>
#include <cstdio>
#include <cstdarg>

namespace rf {
namespace log {

constexpr int LOG_ALL     = 0;
constexpr int LOG_TRACE   = 1;
constexpr int LOG_DEBUG   = 2;
constexpr int LOG_INFO    = 3;
constexpr int LOG_WARNING = 4;
constexpr int LOG_ERROR   = 5;
constexpr int LOG_FATAL   = 6;
constexpr int LOG_NONE    = 7;

// Current global log level (can be overridden in Phase 1)
inline int& global_log_level() {
    static int level = LOG_INFO;
    return level;
}

inline void SetTraceLogLevel(int level) {
    global_log_level() = level;
}

using TraceLogCallback = void(*)(int logLevel, const char* text, va_list args);

inline TraceLogCallback& global_trace_callback() {
    static TraceLogCallback cb = nullptr;
    return cb;
}

inline void SetTraceLogCallback(TraceLogCallback callback) {
    global_trace_callback() = callback;
}

inline double GetTime() {
    // Simple monotonic time via <chrono> — good enough for Phase 0
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start).count();
}

} // namespace log
} // namespace rf

// Bring into global scope for backward compatibility
using namespace rf::log;

// Must be after the using-declaration so the function sees the constants
#include <chrono>

inline void TraceLog(int logLevel, const char* text, ...) {
    if (logLevel < rf::log::global_log_level()) return;

    auto cb = rf::log::global_trace_callback();
    va_list args;
    va_start(args, text);
    if (cb) {
        cb(logLevel, text, args);
    } else {
        const char* level_str = "INFO";
        switch (logLevel) {
            case LOG_ALL:     level_str = "ALL";   break;
            case LOG_TRACE:   level_str = "TRACE"; break;
            case LOG_DEBUG:   level_str = "DEBUG"; break;
            case LOG_INFO:    level_str = "INFO";  break;
            case LOG_WARNING: level_str = "WARN";  break;
            case LOG_ERROR:   level_str = "ERROR"; break;
            case LOG_FATAL:   level_str = "FATAL"; break;
            case LOG_NONE:    level_str = "NONE";  break;
            default:          level_str = "INFO";  break;
        }
        std::fprintf(stderr, "[%s] ", level_str);
        std::vfprintf(stderr, text, args);
        std::fputc('\n', stderr);
    }
    va_end(args);
}
