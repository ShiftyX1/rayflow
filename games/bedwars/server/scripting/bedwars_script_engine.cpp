#include "bedwars_script_engine.hpp"
#include "bedwars_api.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace bedwars::scripting {

BedWarsScriptEngine::BedWarsScriptEngine() = default;
BedWarsScriptEngine::~BedWarsScriptEngine() = default;

bool BedWarsScriptEngine::init() {
    // Use default map sandbox configuration
    return ScriptEngineBase::init(engine::scripting::SandboxConfig::default_for_maps());
}

std::vector<ScriptCommand> BedWarsScriptEngine::take_commands() {
    std::vector<ScriptCommand> result;
    result.swap(pendingCommands_);
    return result;
}

void BedWarsScriptEngine::queue_command(ScriptCommand cmd) {
    pendingCommands_.push_back(std::move(cmd));
}

void BedWarsScriptEngine::register_game_api(engine::scripting::LuaState& lua) {
    api_ = std::make_unique<BedWarsAPI>(*this);
    api_->register_api(lua.state());
}

void BedWarsScriptEngine::register_constants(engine::scripting::LuaState& lua) {
    BlockTypes::register_constants(lua.state());
    TeamConstants::register_constants(lua.state());
}

// Event implementations
void BedWarsScriptEngine::on_player_join(std::uint32_t playerId) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_player_join")) {
        auto result = lua_state()->call("on_player_join", playerId);
        if (!result && log_callback()) {
            log_callback()("[script error] on_player_join: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_player_leave(std::uint32_t playerId) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_player_leave")) {
        auto result = lua_state()->call("on_player_leave", playerId);
        if (!result && log_callback()) {
            log_callback()("[script error] on_player_leave: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_player_spawn(std::uint32_t playerId, float x, float y, float z) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_player_spawn")) {
        lua_state()->state()["on_player_spawn"](playerId, x, y, z);
    }
}

void BedWarsScriptEngine::on_player_death(std::uint32_t playerId, std::uint32_t killerId) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_player_death")) {
        lua_state()->state()["on_player_death"](playerId, killerId);
    }
}

void BedWarsScriptEngine::on_block_break(std::uint32_t playerId, int x, int y, int z, int blockType) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_block_break")) {
        auto result = lua_state()->call("on_block_break", playerId, x, y, z, blockType);
        if (!result && log_callback()) {
            log_callback()("[script error] on_block_break: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_block_place(std::uint32_t playerId, int x, int y, int z, int blockType) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_block_place")) {
        auto result = lua_state()->call("on_block_place", playerId, x, y, z, blockType);
        if (!result && log_callback()) {
            log_callback()("[script error] on_block_place: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_round_start(int roundNumber) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_round_start")) {
        auto result = lua_state()->call("on_round_start", roundNumber);
        if (!result && log_callback()) {
            log_callback()("[script error] on_round_start: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_round_end(int winningTeam) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_round_end")) {
        auto result = lua_state()->call("on_round_end", winningTeam);
        if (!result && log_callback()) {
            log_callback()("[script error] on_round_end: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_match_start() {
    call_hook("on_match_start");
}

void BedWarsScriptEngine::on_match_end(int winningTeam) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_match_end")) {
        auto result = lua_state()->call("on_match_end", winningTeam);
        if (!result && log_callback()) {
            log_callback()("[script error] on_match_end: " + result.error);
        }
    }
}

void BedWarsScriptEngine::on_custom_event(const std::string& eventName, const std::string& data) {
    if (!has_scripts() || !lua_state()) return;
    
    if (lua_state()->has_function("on_custom")) {
        lua_state()->state()["on_custom"](eventName, data);
    }
}

} // namespace bedwars::scripting
