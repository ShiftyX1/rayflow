#pragma once

// =============================================================================
// ScriptAPIModule — Interface for modular API registration
// Games create modules implementing this interface and register them
// via ScriptEngineBase::register_api_module().
// =============================================================================

#include <string>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/forward.hpp>

namespace engine::scripting {

/// Abstract interface for a Lua API module.
/// Each module registers its functions under a named namespace table.
///
/// ## Example:
///   ```
///   class GameModule : public ScriptAPIModule {
///       std::string name() const override { return "game"; }
///       void register_api(sol::state& lua, sol::table& ns) override {
///           ns["broadcast"] = [](const std::string& msg) { ... };
///       }
///   };
///   ```
class ScriptAPIModule {
public:
    virtual ~ScriptAPIModule() = default;

    /// Unique module name (becomes the Lua namespace, e.g. "game", "world")
    virtual std::string name() const = 0;

    /// Register API functions into the given namespace table.
    /// @param lua   Full sol::state (for creating usertypes, etc.)
    /// @param ns    The table for this module's namespace (already created)
    virtual void register_api(sol::state& lua, sol::table& ns) = 0;

    /// Human-readable description (for auto-documentation / introspection)
    virtual std::string description() const { return ""; }
};

} // namespace engine::scripting
