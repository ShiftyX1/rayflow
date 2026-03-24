#pragma once

// =============================================================================
// convar_registry.hpp — Typed key-value store for console variables (ConVars).
//
// Games register variables with name, default value, description, and flags.
// The console Lua API exposes cvar.get/set/list/reset/describe.
// Access-controlled: games provide a hook to block server-only changes on
// non-host clients.
// =============================================================================

#include "engine/core/export.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace engine::console {

// ConVar flags (bit field).
enum class ConVarFlags : std::uint32_t {
    None    = 0,
    Client  = 1 << 0,  // Client-side variable
    Server  = 1 << 1,  // Server-side variable (only host can modify)
    Cheat   = 1 << 2,  // Requires sv_cheats
    Archive = 1 << 3,  // Saved/restored in user config
};

inline ConVarFlags  operator|(ConVarFlags a, ConVarFlags b) { return static_cast<ConVarFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)); }
inline ConVarFlags  operator&(ConVarFlags a, ConVarFlags b) { return static_cast<ConVarFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)); }
inline bool         has_flag(ConVarFlags v, ConVarFlags f)  { return (static_cast<std::uint32_t>(v) & static_cast<std::uint32_t>(f)) != 0; }

using ConVarValue = std::variant<int, float, bool, std::string>;

struct ConVarEntry {
    std::string  name;
    ConVarValue  value;
    ConVarValue  defaultValue;
    std::string  description;
    ConVarFlags  flags{ConVarFlags::None};
};

class RAYFLOW_CORE_API ConVarRegistry {
public:
    /// Callback invoked when a cvar value changes: (name, new_entry).
    using ChangeCallback = std::function<void(const std::string&, const ConVarEntry&)>;

    /// Access-control hook: returns true if the caller is allowed to set this cvar.
    /// Games set this to check e.g. "is host" for Server-flagged cvars.
    using AccessHook = std::function<bool(const std::string& name, ConVarFlags flags)>;

    ConVarRegistry();
    ~ConVarRegistry();

    /// Register a cvar.  If it already exists the call is ignored.
    void register_cvar(const std::string& name, ConVarValue defaultValue,
                       const std::string& description = {},
                       ConVarFlags flags = ConVarFlags::Client);

    /// Get a cvar entry.  Returns nullopt if not registered.
    std::optional<ConVarEntry> get(const std::string& name) const;

    /// Get the value only.  Returns nullopt if not registered.
    std::optional<ConVarValue> get_value(const std::string& name) const;

    /// Set a cvar value.  Returns false if access denied or name unknown.
    bool set(const std::string& name, ConVarValue value);

    /// Reset a cvar to its default value.
    bool reset(const std::string& name);

    /// List all registered cvar names (optionally filtered by substring).
    std::vector<std::string> list(const std::string& filter = {}) const;

    /// Set the change callback.
    void set_change_callback(ChangeCallback cb);

    /// Set the access-control hook.
    void set_access_hook(AccessHook hook);

private:
    mutable std::mutex mu_;
    std::unordered_map<std::string, ConVarEntry> cvars_;
    ChangeCallback changeCb_;
    AccessHook accessHook_;
};

} // namespace engine::console
