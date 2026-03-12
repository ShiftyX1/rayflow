#include "timer_api.hpp"
#include "engine/core/scripting/script_engine_base.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <atomic>

namespace engine::scripting::api {

namespace {
    std::atomic<int> g_anonymousTimerCounter{0};
}

void register_timer_api(sol::state& lua, ScriptEngineBase& engine) {
    auto timer = lua.create_named_table("timer");

    // timer.after(delay, callback) — one-shot timer
    timer["after"] = [&lua, &engine](double delaySec, sol::function callback) {
        std::string funcName = "__timer_cb_" + std::to_string(++g_anonymousTimerCounter);
        lua[funcName] = callback;
        engine.add_timer(funcName, delaySec, 0.0, funcName);
    };

    // timer.every(interval, callback) — repeating timer
    timer["every"] = [&lua, &engine](double intervalSec, sol::function callback) {
        std::string funcName = "__timer_cb_" + std::to_string(++g_anonymousTimerCounter);
        lua[funcName] = callback;
        engine.add_timer(funcName, intervalSec, intervalSec, funcName);
    };

    // timer.named(name, delay, callback) — named one-shot timer
    timer["named"] = [&lua, &engine](const std::string& name, double delaySec, sol::function callback) {
        std::string funcName = "__timer_named_" + name;
        lua[funcName] = callback;
        engine.add_timer(name, delaySec, 0.0, funcName);
    };

    // timer.cancel(name) — cancel a named timer
    timer["cancel"] = [&engine](const std::string& name) {
        engine.cancel_timer(name);
    };
}

} // namespace engine::scripting::api
