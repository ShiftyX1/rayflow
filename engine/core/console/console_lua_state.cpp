#include "console_lua_state.hpp"
#include "console_log_sink.hpp"

#include "engine/core/scripting/lua_state.hpp"
#include "engine/core/scripting/api/engine_api.hpp"
#include "engine/core/logging.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace engine::console {

struct ConsoleLuaState::Impl {
    std::unique_ptr<engine::scripting::LuaState> lua;
    ConsoleLogSink* sink{nullptr};
};

ConsoleLuaState::ConsoleLuaState() : impl_(std::make_unique<Impl>()) {}
ConsoleLuaState::~ConsoleLuaState() = default;

bool ConsoleLuaState::init(ConsoleLogSink& sink) {
    impl_->sink = &sink;
    impl_->lua = engine::scripting::create_engine_state();
    if (!impl_->lua) {
        TraceLog(LOG_ERROR, "[console] Failed to create Lua state");
        return false;
    }

    // Register engine.* API with output routed to the console sink
    auto logCb = [&sink](const std::string& msg) {
        sink.push(LOG_INFO, msg);
    };
    engine::scripting::api::register_engine_api(
        impl_->lua->state(), std::move(logCb), nullptr);

    // Convenience: clear() to clear the console
    impl_->lua->state().set_function("clear", [&sink]() { sink.clear(); });

    TraceLog(LOG_INFO, "[console] Lua state initialised");
    return true;
}

std::string ConsoleLuaState::execute(std::string_view command) {
    if (!impl_->lua) return "error: Lua state not initialised";

    // Try as expression first (prefix with "return ") to capture the result
    std::string expr = "return " + std::string(command);
    auto result = impl_->lua->state().safe_script(
        expr, sol::script_pass_on_error);

    if (!result.valid()) {
        // Fall back to executing as statement
        result = impl_->lua->state().safe_script(
            std::string(command), sol::script_pass_on_error);
    }

    if (!result.valid()) {
        sol::error err = result;
        return err.what();
    }

    // Convert result to string if non-nil
    sol::object obj = result;
    if (obj.is<std::nullptr_t>() || obj.get_type() == sol::type::none ||
        obj.get_type() == sol::type::lua_nil) {
        return {};
    }

    // Use Lua's tostring for the result
    sol::state_view sv = impl_->lua->state();
    sol::protected_function tostr = sv["tostring"];
    if (tostr.valid()) {
        auto ts = tostr(obj);
        if (ts.valid()) {
            sol::object r = ts;
            if (r.is<std::string>()) return r.as<std::string>();
        }
    }
    return {};
}

void ConsoleLuaState::register_api(std::function<void(sol::state&)> registrar) {
    if (impl_->lua && registrar) {
        registrar(impl_->lua->state());
    }
}

sol::state& ConsoleLuaState::state() {
    return impl_->lua->state();
}

} // namespace engine::console
