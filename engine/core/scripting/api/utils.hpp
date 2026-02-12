#pragma once

// =============================================================================
// Scripting Utilities — shared helpers for API modules
// =============================================================================

#include <string>

namespace sol {
struct variadic_args;
struct this_state;
}

namespace engine::scripting::api {

/// Convert Lua variadic arguments to a tab-separated string.
/// Uses Lua's tostring() for each argument — the canonical pattern
/// previously duplicated across log/print implementations.
std::string lua_args_to_string(sol::variadic_args va, sol::this_state ts);

} // namespace engine::scripting::api
