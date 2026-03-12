#pragma once

// =============================================================================
// LuaState::call() variadic template — requires sol2 headers.
// Include this header in .cpp files that need to call Lua functions
// with an arbitrary number of arguments.
//
// Usage:
//   #include <engine/core/scripting/lua_state_call.hpp>
//   state.call("on_player_spawn", playerId, x, y, z);
// =============================================================================

#include "lua_state.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace engine::scripting {

template<typename... Args>
ScriptResult LuaState::call(const std::string& funcName, Args&&... args) {
    if (sandboxed_) {
        reset_limiter();
    }
    
    sol::protected_function func = state()[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(std::forward<Args>(args)...);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

} // namespace engine::scripting
