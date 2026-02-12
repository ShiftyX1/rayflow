#pragma once

#include <engine/core/scripting/script_engine_base.hpp>

#include <memory>
#include <string>

namespace bedwars::scripting {

/// Client-side script engine for BedWars.
/// Loads game scripts from scripts/client/ via VFS.
/// Provides client-specific API (log, UI control, effects, etc.)
class ClientScriptEngine : public engine::scripting::ScriptEngineBase {
public:
    ClientScriptEngine();
    ~ClientScriptEngine() override;
    
    /// Initialize with client sandbox configuration
    bool init();
    
    /// Load game-level scripts from VFS (scripts/client/)
    bool init_game_scripts();

protected:
    void register_game_api(engine::scripting::LuaState& lua) override;
    void register_constants(engine::scripting::LuaState& lua) override;
};

} // namespace bedwars::scripting
