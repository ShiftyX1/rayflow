#include "logging.hpp"

#include <chrono>
#include <cstdio>

namespace rf {
namespace log {

int& global_log_level() {
    static int level = LOG_INFO;
    return level;
}

void SetTraceLogLevel(int level) {
    global_log_level() = level;
}

TraceLogCallback& global_trace_callback() {
    static TraceLogCallback cb = nullptr;
    return cb;
}

void SetTraceLogCallback(TraceLogCallback callback) {
    global_trace_callback() = callback;
}

double GetTime() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start).count();
}

} // namespace log
} // namespace rf

void TraceLog(int logLevel, const char* text, ...) {
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
