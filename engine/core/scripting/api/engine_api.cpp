#include "engine_api.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "utils.hpp"

#include <chrono>

namespace engine::scripting::api {

namespace {
    const auto g_engineStartTime = std::chrono::steady_clock::now();
    constexpr const char* kEngineVersion = "0.1.0";
}

void register_engine_api(sol::state& lua,
                         std::function<void(const std::string&)> logCallback,
                         std::function<float()> getDeltaTime) {
    auto eng = lua.create_named_table("engine");

    // engine.log(...) — formatted logging (also exposed as global log/print)
    auto logFn = [logCallback](sol::variadic_args va, sol::this_state ts) {
        if (logCallback) {
            logCallback(lua_args_to_string(va, ts));
        }
    };

    eng["log"] = logFn;

    // engine.time() — seconds since engine start
    eng["time"] = []() -> double {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - g_engineStartTime).count();
    };

    // engine.delta_time() — last frame/tick delta
    eng["delta_time"] = [getDeltaTime]() -> float {
        return getDeltaTime ? getDeltaTime() : 0.0f;
    };

    // engine.version() — engine version string
    eng["version"] = []() -> std::string {
        return kEngineVersion;
    };

    // Global convenience aliases
    lua["log"] = logFn;
    lua["print"] = logFn;
}

} // namespace engine::scripting::api
