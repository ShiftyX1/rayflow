#pragma once

// Engine Scripting Module - Lua scripting infrastructure
// 
// This module provides:
// - LuaState: Wrapper around sol2/LuaJIT with sandbox support
// - Sandbox: Security utilities for untrusted scripts
// - ScriptEngineBase: Abstract base class for game-specific engines
//
// Usage:
//   1. Create a derived class from ScriptEngineBase
//   2. Override register_game_api() to expose your game's API
//   3. Override register_constants() to expose game constants (BLOCK.*, etc.)
//
// Example:
//   class MyGameScriptEngine : public engine::scripting::ScriptEngineBase {
//   protected:
//       void register_game_api(LuaState& lua) override {
//           auto& state = lua.state();
//           state["game"] = state.create_table_with(
//               "broadcast", [this](const std::string& msg) { ... }
//           );
//       }
//   };

/*
    Pulse Studio developed this module. 2025.
    Licensed under the MIT License.
    Feel free to use and modify it as needed.
*/

#include "lua_state.hpp"
#include "sandbox.hpp"
#include "script_types.hpp"
#include "script_engine_base.hpp"
