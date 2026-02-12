#include "client_script_engine.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace bedwars::scripting {

ClientScriptEngine::ClientScriptEngine() = default;
ClientScriptEngine::~ClientScriptEngine() = default;

bool ClientScriptEngine::init() {
    // Use game scripts sandbox configuration (same limits as server)
    return ScriptEngineBase::init(engine::scripting::SandboxConfig::default_for_game_scripts());
}

bool ClientScriptEngine::init_game_scripts() {
    auto result = load_game_scripts("scripts/client");
    if (!result) {
        if (log_callback()) {
            log_callback()("[client] Failed to load game scripts: " + result.error);
        }
        return false;
    }
    if (has_game_scripts()) {
        if (log_callback()) {
            log_callback()("[client] Game scripts loaded from scripts/client/");
        }
    }
    return true;
}

void ClientScriptEngine::register_game_api(engine::scripting::LuaState& lua) {
    auto& state = lua.state();
    
    // Client log function — uses the log callback set by BedWarsClient
    state["log"] = [this](sol::variadic_args va, sol::this_state ts) {
        sol::state_view L(ts);
        std::ostringstream oss;
        bool first = true;
        for (auto v : va) {
            if (!first) oss << "\t";
            first = false;
            sol::protected_function tostring = L["tostring"];
            if (tostring.valid()) {
                auto r = tostring(v);
                if (r.valid()) {
                    oss << r.get<std::string>();
                }
            }
        }
        if (log_callback()) {
            log_callback()("[script] " + oss.str());
        }
    };
    
    state["print"] = state["log"];
}

void ClientScriptEngine::register_constants(engine::scripting::LuaState& /*lua*/) {
    // Client-side constants can be added here in the future
}

} // namespace bedwars::scripting
