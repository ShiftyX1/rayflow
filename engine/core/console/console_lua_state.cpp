#include "console_lua_state.hpp"
#include "console_log_sink.hpp"
#include "convar_registry.hpp"

#include "engine/core/scripting/lua_state.hpp"
#include "engine/core/scripting/sandbox.hpp"
#include "engine/core/scripting/api/engine_api.hpp"
#include "engine/core/scripting/api/math_api.hpp"
#include "engine/core/scripting/api/timer_api.hpp"
#include "engine/core/scripting/api/events_api.hpp"
#include "engine/core/logging.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace engine::console {

// ============================================================================
// Timer entry (same layout as scripting::ScriptTimer, kept local to avoid
// pulling script_engine_base.hpp into the console module).
// ============================================================================

struct ConsoleTimer {
    std::string name;
    double      remainingSec{0.0};
    double      intervalSec{0.0};   // 0 = one-shot
    std::string callbackFunc;
    bool        cancelled{false};
};

// ============================================================================
// Registered API module descriptor (replayed on state reset).
// ============================================================================

struct RegisteredModule {
    std::string name;
    std::function<void(sol::state&, sol::table&)> registrar;
};

// ============================================================================
// Impl
// ============================================================================

struct ConsoleLuaState::Impl {
    std::unique_ptr<engine::scripting::LuaState> lua;
    ConsoleLogSink*   sink{nullptr};
    ConVarRegistry*   cvars{nullptr};
    std::filesystem::path userScriptsDir;
    float             currentDt{0.0f};

    // Timers
    std::vector<ConsoleTimer> timers;
    engine::scripting::api::ITimerHost timerHost;

    // Registered API modules (for replay on reset)
    std::vector<RegisteredModule> modules;

    // Raw registrars (register_api — legacy)
    std::vector<std::function<void(sol::state&)>> rawRegistrars;

    // ------------------------------------------------------------------
    // Timer host wiring
    // ------------------------------------------------------------------
    void init_timer_host() {
        timerHost.add_timer = [this](const std::string& name, double delay,
                                     double interval, const std::string& cb) {
            // Cancel existing with same name
            for (auto& t : timers) {
                if (t.name == name) t.cancelled = true;
            }
            timers.push_back({name, delay, interval, cb, false});
        };
        timerHost.cancel_timer = [this](const std::string& name) {
            for (auto& t : timers) {
                if (t.name == name) t.cancelled = true;
            }
        };
    }

    // ------------------------------------------------------------------
    // Tick timers
    // ------------------------------------------------------------------
    void update_timers(float dt) {
        if (!lua) return;
        for (auto& t : timers) {
            if (t.cancelled) continue;
            t.remainingSec -= static_cast<double>(dt);
            if (t.remainingSec <= 0.0) {
                // Fire
                sol::protected_function fn = lua->state()[t.callbackFunc];
                if (fn.valid()) {
                    auto res = fn();
                    if (!res.valid()) {
                        sol::error err = res;
                        if (sink) sink->push(LOG_ERROR, std::string("[timer] ") + err.what());
                    }
                }
                if (t.intervalSec > 0.0) {
                    t.remainingSec = t.intervalSec;
                } else {
                    t.cancelled = true;
                }
            }
        }
        // Remove cancelled
        timers.erase(
            std::remove_if(timers.begin(), timers.end(),
                           [](const ConsoleTimer& t) { return t.cancelled; }),
            timers.end());
    }

    // ------------------------------------------------------------------
    // Register ConVar Lua API
    // ------------------------------------------------------------------
    void register_cvar_api() {
        if (!lua || !cvars) return;
        auto& state = lua->state();
        auto tbl = state.create_named_table("cvar");

        tbl["get"] = [this](const std::string& name) -> sol::object {
            auto val = cvars->get_value(name);
            if (!val) return sol::make_object(lua->state(), sol::lua_nil);
            return std::visit([this](auto&& v) -> sol::object {
                return sol::make_object(lua->state(), v);
            }, *val);
        };

        tbl["set"] = [this](const std::string& name, sol::object value) -> bool {
            ConVarValue cv;
            if (value.is<bool>())             cv = value.as<bool>();
            else if (value.is<int>())         cv = value.as<int>();
            else if (value.is<float>())       cv = value.as<float>();
            else if (value.is<double>())      cv = static_cast<float>(value.as<double>());
            else if (value.is<std::string>()) cv = value.as<std::string>();
            else {
                if (sink) sink->push(LOG_ERROR, "cvar.set: unsupported value type");
                return false;
            }
            bool ok = cvars->set(name, std::move(cv));
            if (!ok && sink) {
                sink->push(LOG_ERROR, "cvar.set: failed (unknown name or access denied): " + name);
            }
            return ok;
        };

        tbl["list"] = [this](sol::optional<std::string> filter) -> sol::table {
            auto names = cvars->list(filter.value_or(""));
            sol::table result = lua->state().create_table(static_cast<int>(names.size()), 0);
            for (std::size_t i = 0; i < names.size(); ++i) {
                result[static_cast<int>(i + 1)] = names[i];
            }
            return result;
        };

        tbl["reset"] = [this](const std::string& name) -> bool {
            return cvars->reset(name);
        };

        tbl["describe"] = [this](const std::string& name) -> std::string {
            auto entry = cvars->get(name);
            if (!entry) return "unknown cvar: " + name;
            std::ostringstream oss;
            oss << entry->name;
            oss << " = ";
            std::visit([&oss](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>)
                    oss << (v ? "true" : "false");
                else if constexpr (std::is_same_v<T, std::string>)
                    oss << "\"" << v << "\"";
                else
                    oss << v;
            }, entry->value);
            oss << "  (default: ";
            std::visit([&oss](auto&& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>)
                    oss << (v ? "true" : "false");
                else if constexpr (std::is_same_v<T, std::string>)
                    oss << "\"" << v << "\"";
                else
                    oss << v;
            }, entry->defaultValue);
            oss << ")";
            if (!entry->description.empty()) {
                oss << "  — " << entry->description;
            }
            return oss.str();
        };
    }

    // ------------------------------------------------------------------
    // exec(path) — load & execute a .lua file from user_scripts dir
    // ------------------------------------------------------------------
    std::string exec_file(const std::string& relativePath) {
        if (userScriptsDir.empty()) {
            return "error: user scripts directory not configured";
        }

        // Security: only allow .lua extension
        if (relativePath.size() < 4 ||
            relativePath.substr(relativePath.size() - 4) != ".lua") {
            return "error: only .lua files are allowed";
        }

        // Build candidate path and canonicalize
        std::filesystem::path candidate = userScriptsDir / relativePath;
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(candidate, ec);
        if (ec) return "error: invalid path";

        // Security: ensure resolved path is inside userScriptsDir
        auto baseCanonical = std::filesystem::weakly_canonical(userScriptsDir, ec);
        if (ec) return "error: invalid base path";

        auto rel = canonical.lexically_relative(baseCanonical);
        if (rel.empty() || rel.native().starts_with(L"..")) {
            return "error: access denied (path escapes user_scripts directory)";
        }

        // Read file
        std::ifstream file(canonical, std::ios::in);
        if (!file.is_open()) {
            return "error: file not found: " + relativePath;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        std::string script = ss.str();

        // Validate
        auto validation = engine::scripting::Sandbox::validate_script(script);
        if (!validation) {
            std::string errors;
            for (const auto& e : validation.errors) {
                if (!errors.empty()) errors += "; ";
                errors += e;
            }
            return "error: script validation failed: " + errors;
        }

        // Reset limiter before execution
        lua->reset_limiter();

        // Execute
        auto result = lua->state().safe_script(script, sol::script_pass_on_error,
                                                relativePath);
        if (!result.valid()) {
            sol::error err = result;
            return std::string(err.what());
        }
        return {};
    }

    // ------------------------------------------------------------------
    // help() — list available namespaces and commands
    // ------------------------------------------------------------------
    void register_help(ConsoleLogSink& logSink) {
        lua->state().set_function("help", [this, &logSink]() {
            logSink.push(LOG_INFO, "=== Console Commands ===");
            logSink.push(LOG_INFO, "  clear()                  — clear console output");
            logSink.push(LOG_INFO, "  help()                   — show this help");
            logSink.push(LOG_INFO, "  exec(\"file.lua\")         — execute a user script");
            logSink.push(LOG_INFO, "");
            logSink.push(LOG_INFO, "=== API Namespaces ===");
            logSink.push(LOG_INFO, "  engine.log(...)          — log a message");
            logSink.push(LOG_INFO, "  engine.time()            — seconds since start");
            logSink.push(LOG_INFO, "  engine.delta_time()      — last frame delta");
            logSink.push(LOG_INFO, "  engine.version()         — engine version");
            logSink.push(LOG_INFO, "  math.lerp/clamp/remap    — math utilities");
            logSink.push(LOG_INFO, "  vec2(x,y) / vec3(x,y,z)  — vector types");
            logSink.push(LOG_INFO, "  timer.after/every/cancel — timers");
            logSink.push(LOG_INFO, "  events.on/emit/off/clear — event bus");
            if (cvars) {
                logSink.push(LOG_INFO, "  cvar.get/set/list/reset/describe — console variables");
            }
            if (!modules.empty()) {
                logSink.push(LOG_INFO, "");
                logSink.push(LOG_INFO, "=== Game Modules ===");
                for (const auto& m : modules) {
                    logSink.push(LOG_INFO, "  " + m.name + ".*");
                }
            }
        });
    }
};

// ============================================================================
// Public API
// ============================================================================

ConsoleLuaState::ConsoleLuaState() : impl_(std::make_unique<Impl>()) {}
ConsoleLuaState::~ConsoleLuaState() = default;

bool ConsoleLuaState::init(ConsoleLogSink& sink,
                           const std::filesystem::path& userScriptsDir,
                           ConVarRegistry* cvars) {
    impl_->sink = &sink;
    impl_->cvars = cvars;
    impl_->userScriptsDir = userScriptsDir;

    // Create sandboxed state
    auto cfg = engine::scripting::SandboxConfig::default_for_console();
    cfg.printHandler = [&sink](const std::string& msg) {
        sink.push(LOG_INFO, msg);
    };
    impl_->lua = engine::scripting::Sandbox::create(cfg);
    if (!impl_->lua) {
        TraceLog(LOG_ERROR, "[console] Failed to create sandboxed Lua state");
        return false;
    }

    // Wire timer host
    impl_->init_timer_host();

    // ---- Register all base APIs ----

    // 1. engine.* (log → console sink)
    auto logCb = [&sink](const std::string& msg) {
        sink.push(LOG_INFO, msg);
    };
    auto getDt = [this]() -> float { return impl_->currentDt; };
    engine::scripting::api::register_engine_api(impl_->lua->state(), logCb, getDt);

    // 2. math (vec2, vec3, lerp, clamp, remap)
    engine::scripting::api::register_math_api(impl_->lua->state());

    // 3. timer.after / timer.every / timer.cancel
    engine::scripting::api::register_timer_api(impl_->lua->state(), impl_->timerHost);

    // 4. events.on / events.emit / events.off / events.clear
    engine::scripting::api::register_events_api(impl_->lua->state());

    // 5. cvar.* (if registry provided)
    impl_->register_cvar_api();

    // 6. Utility commands
    impl_->lua->state().set_function("clear", [&sink]() { sink.clear(); });

    // exec(path) — load user script
    impl_->lua->state().set_function("exec", [this](const std::string& path) -> std::string {
        std::string result = impl_->exec_file(path);
        if (!result.empty()) {
            impl_->sink->push(LOG_ERROR, result);
        } else {
            impl_->sink->push(LOG_INFO, "executed: " + path);
        }
        return result;
    });

    // help()
    impl_->register_help(sink);

    // Replay any API modules registered before init()
    for (auto& m : impl_->modules) {
        sol::table ns = impl_->lua->state().create_named_table(m.name);
        m.registrar(impl_->lua->state(), ns);
    }
    for (auto& r : impl_->rawRegistrars) {
        r(impl_->lua->state());
    }

    TraceLog(LOG_INFO, "[console] Sandboxed Lua state initialised");

    // ---- Autoexec ----
    if (!userScriptsDir.empty()) {
        auto autoexecPath = userScriptsDir / "autoexec.lua";
        std::error_code ec;
        if (std::filesystem::exists(autoexecPath, ec) && !ec) {
            std::string result = impl_->exec_file("autoexec.lua");
            if (result.empty()) {
                sink.push(LOG_INFO, "[console] autoexec.lua executed");
            } else {
                sink.push(LOG_WARNING, "[console] autoexec.lua: " + result);
            }
        }
    }

    return true;
}

std::string ConsoleLuaState::execute(std::string_view command) {
    if (!impl_->lua) return "error: Lua state not initialised";

    // Reset execution limiter before each command
    impl_->lua->reset_limiter();

    // Try as expression first (prefix with "return ") to capture the result
    std::string expr = "return " + std::string(command);
    auto result = impl_->lua->state().safe_script(
        expr, sol::script_pass_on_error);

    if (!result.valid()) {
        // Fall back to executing as statement
        impl_->lua->reset_limiter();
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

void ConsoleLuaState::update(float dt) {
    impl_->currentDt = dt;
    impl_->update_timers(dt);
}

void ConsoleLuaState::register_api(std::function<void(sol::state&)> registrar) {
    if (registrar) {
        impl_->rawRegistrars.push_back(registrar);
        if (impl_->lua) {
            registrar(impl_->lua->state());
        }
    }
}

void ConsoleLuaState::register_api_module(
    const std::string& namespaceName,
    std::function<void(sol::state&, sol::table&)> registrar) {
    impl_->modules.push_back({namespaceName, registrar});
    if (impl_->lua) {
        sol::table ns = impl_->lua->state().create_named_table(namespaceName);
        registrar(impl_->lua->state(), ns);
    }
}

std::vector<std::string> ConsoleLuaState::list_api_modules() const {
    std::vector<std::string> names;
    names.reserve(impl_->modules.size());
    for (const auto& m : impl_->modules) {
        names.push_back(m.name);
    }
    return names;
}

sol::state& ConsoleLuaState::state() {
    return impl_->lua->state();
}

} // namespace engine::console
