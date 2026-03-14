#pragma once

// =============================================================================
// logging.hpp — Logging facade (replaces raylib TraceLog / LOG_* constants)
//
// All mutable global state lives in logging.cpp (engine_core DLL) to ensure
// a single copy across shared library boundaries.
// =============================================================================

#include "export.hpp"

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

// Global log level — single process-wide instance (lives in engine_core)
RAYFLOW_CORE_API int& global_log_level();
RAYFLOW_CORE_API void SetTraceLogLevel(int level);

using TraceLogCallback = void(*)(int logLevel, const char* text, va_list args);

RAYFLOW_CORE_API TraceLogCallback& global_trace_callback();
RAYFLOW_CORE_API void SetTraceLogCallback(TraceLogCallback callback);

// Monotonic time since first call (single timebase across all DLLs)
RAYFLOW_CORE_API double GetTime();

} // namespace log
} // namespace rf

// Bring into global scope for backward compatibility
using namespace rf::log;

// TraceLog — the main logging entry point
RAYFLOW_CORE_API void TraceLog(int logLevel, const char* text, ...);
