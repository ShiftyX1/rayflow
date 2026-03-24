#include "console_log_sink.hpp"

#include "engine/core/logging.hpp"

namespace engine::console {

ConsoleLogSink::ConsoleLogSink(std::size_t capacity)
    : capacity_(capacity) {
    buffer_.resize(capacity_);
}

void ConsoleLogSink::push(int level, const std::string& message) {
    std::lock_guard lock(mu_);
    if (!enabled_) return;

    auto& entry   = buffer_[head_];
    entry.timestamp = rf::log::GetTime();
    entry.level     = level;
    entry.message   = message;

    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) ++count_;
}

void ConsoleLogSink::snapshot(std::vector<LogEntry>& out) const {
    std::lock_guard lock(mu_);
    out.clear();
    out.reserve(count_);

    if (count_ == 0) return;

    // Oldest entry index
    std::size_t start = (count_ < capacity_) ? 0 : head_;
    for (std::size_t i = 0; i < count_; ++i) {
        out.push_back(buffer_[(start + i) % capacity_]);
    }
}

void ConsoleLogSink::clear() {
    std::lock_guard lock(mu_);
    count_ = 0;
    head_  = 0;
}

void ConsoleLogSink::set_enabled(bool v) {
    std::lock_guard lock(mu_);
    enabled_ = v;
}

bool ConsoleLogSink::enabled() const {
    std::lock_guard lock(mu_);
    return enabled_;
}

} // namespace engine::console
