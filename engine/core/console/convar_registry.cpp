#include "convar_registry.hpp"

#include <algorithm>

namespace engine::console {

ConVarRegistry::ConVarRegistry() = default;
ConVarRegistry::~ConVarRegistry() = default;

void ConVarRegistry::register_cvar(const std::string& name, ConVarValue defaultValue,
                                    const std::string& description, ConVarFlags flags) {
    std::lock_guard lock(mu_);
    if (cvars_.count(name)) return;  // already registered

    ConVarEntry entry;
    entry.name = name;
    entry.value = defaultValue;
    entry.defaultValue = defaultValue;
    entry.description = description;
    entry.flags = flags;
    cvars_.emplace(name, std::move(entry));
}

std::optional<ConVarEntry> ConVarRegistry::get(const std::string& name) const {
    std::lock_guard lock(mu_);
    auto it = cvars_.find(name);
    if (it == cvars_.end()) return std::nullopt;
    return it->second;
}

std::optional<ConVarValue> ConVarRegistry::get_value(const std::string& name) const {
    std::lock_guard lock(mu_);
    auto it = cvars_.find(name);
    if (it == cvars_.end()) return std::nullopt;
    return it->second.value;
}

bool ConVarRegistry::set(const std::string& name, ConVarValue value) {
    std::lock_guard lock(mu_);
    auto it = cvars_.find(name);
    if (it == cvars_.end()) return false;

    // Access control
    if (accessHook_ && !accessHook_(name, it->second.flags)) {
        return false;
    }

    it->second.value = std::move(value);

    if (changeCb_) {
        changeCb_(name, it->second);
    }
    return true;
}

bool ConVarRegistry::reset(const std::string& name) {
    std::lock_guard lock(mu_);
    auto it = cvars_.find(name);
    if (it == cvars_.end()) return false;

    it->second.value = it->second.defaultValue;
    if (changeCb_) {
        changeCb_(name, it->second);
    }
    return true;
}

std::vector<std::string> ConVarRegistry::list(const std::string& filter) const {
    std::lock_guard lock(mu_);
    std::vector<std::string> result;
    result.reserve(cvars_.size());
    for (const auto& [name, _] : cvars_) {
        if (filter.empty() || name.find(filter) != std::string::npos) {
            result.push_back(name);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ConVarRegistry::set_change_callback(ChangeCallback cb) {
    std::lock_guard lock(mu_);
    changeCb_ = std::move(cb);
}

void ConVarRegistry::set_access_hook(AccessHook hook) {
    std::lock_guard lock(mu_);
    accessHook_ = std::move(hook);
}

} // namespace engine::console
