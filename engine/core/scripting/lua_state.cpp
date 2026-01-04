#include "lua_state.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <chrono>
#include <atomic>

namespace engine::scripting {

namespace {

// Custom allocator for memory tracking
struct MemoryTracker {
    std::atomic<std::size_t> allocated{0};
    std::size_t limit{0};
    
    static void* alloc(void* ud, void* ptr, std::size_t osize, std::size_t nsize) {
        auto* tracker = static_cast<MemoryTracker*>(ud);
        
        if (nsize == 0) {
            // Free
            if (ptr) {
                tracker->allocated -= osize;
                std::free(ptr);
            }
            return nullptr;
        }
        
        if (ptr == nullptr) {
            // Allocate
            if (tracker->limit > 0 && tracker->allocated + nsize > tracker->limit) {
                return nullptr;  // Memory limit exceeded
            }
            void* newPtr = std::malloc(nsize);
            if (newPtr) {
                tracker->allocated += nsize;
            }
            return newPtr;
        }
        
        // Reallocate
        std::size_t delta = nsize > osize ? nsize - osize : 0;
        if (tracker->limit > 0 && delta > 0 && tracker->allocated + delta > tracker->limit) {
            return nullptr;  // Memory limit exceeded
        }
        
        void* newPtr = std::realloc(ptr, nsize);
        if (newPtr) {
            tracker->allocated = tracker->allocated - osize + nsize;
        }
        return newPtr;
    }
};

// Instruction count hook for limiting execution
struct ExecutionLimiter {
    std::size_t instructionCount{0};
    std::size_t maxInstructions{0};
    std::chrono::steady_clock::time_point startTime;
    double maxTimeSec{0.0};
    bool exceeded{false};
    
    static void hook(lua_State* L, lua_Debug*) {
        void* ud = nullptr;
        lua_getallocf(L, &ud);
        // The limiter is stored in registry
        lua_getfield(L, LUA_REGISTRYINDEX, "__exec_limiter");
        auto* limiter = static_cast<ExecutionLimiter*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        
        if (!limiter) return;
        
        limiter->instructionCount++;
        
        if (limiter->maxInstructions > 0 && limiter->instructionCount > limiter->maxInstructions) {
            limiter->exceeded = true;
            luaL_error(L, "instruction limit exceeded");
        }
        
        if (limiter->maxTimeSec > 0.0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - limiter->startTime).count();
            if (elapsed > limiter->maxTimeSec) {
                limiter->exceeded = true;
                luaL_error(L, "execution time limit exceeded");
            }
        }
    }
};

} // namespace

struct LuaState::Impl {
    sol::state lua;
    std::unique_ptr<MemoryTracker> memTracker;
    std::unique_ptr<ExecutionLimiter> execLimiter;
    
    Impl() = default;
    
    bool init_with_allocator(std::size_t memLimit) {
        memTracker = std::make_unique<MemoryTracker>();
        memTracker->limit = memLimit;
        
        lua = sol::state(sol::default_at_panic, MemoryTracker::alloc, memTracker.get());
        
        if (!lua.lua_state()) {
            return false;
        }
        
        return true;
    }
    
    bool init_default() {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::utf8
        );
        return true;
    }
    
    void setup_execution_limiter(const ScriptLimits& limits) {
        execLimiter = std::make_unique<ExecutionLimiter>();
        execLimiter->maxInstructions = limits.maxInstructions;
        execLimiter->maxTimeSec = limits.maxExecutionTimeSec;
        
        // Store limiter in registry for hook access
        lua_pushlightuserdata(lua.lua_state(), execLimiter.get());
        lua_setfield(lua.lua_state(), LUA_REGISTRYINDEX, "__exec_limiter");
        
        // Set hook to fire every 1000 instructions
        lua_sethook(lua.lua_state(), ExecutionLimiter::hook, LUA_MASKCOUNT, 1000);
    }
    
    void reset_execution_limiter() {
        if (execLimiter) {
            execLimiter->instructionCount = 0;
            execLimiter->startTime = std::chrono::steady_clock::now();
            execLimiter->exceeded = false;
        }
    }
};

LuaState::LuaState() : impl_(std::make_unique<Impl>()) {}

LuaState::~LuaState() = default;

LuaState::LuaState(LuaState&&) noexcept = default;
LuaState& LuaState::operator=(LuaState&&) noexcept = default;

bool LuaState::init() {
    return impl_->init_default();
}

void LuaState::apply_sandbox(const ScriptLimits& limits) {
    limits_ = limits;
    sandboxed_ = true;
    
    auto& lua = impl_->lua;
    
    // Remove dangerous libraries/functions
    lua["os"] = sol::lua_nil;
    lua["io"] = sol::lua_nil;
    lua["debug"] = sol::lua_nil;
    lua["loadfile"] = sol::lua_nil;
    lua["dofile"] = sol::lua_nil;
    lua["load"] = sol::lua_nil;       // Can load bytecode
    lua["loadstring"] = sol::lua_nil;
    lua["require"] = sol::lua_nil;    // File system access
    lua["package"] = sol::lua_nil;
    lua["collectgarbage"] = sol::lua_nil;  // Can cause DoS
    lua["rawget"] = sol::lua_nil;
    lua["rawset"] = sol::lua_nil;
    lua["rawequal"] = sol::lua_nil;
    lua["setmetatable"] = sol::lua_nil;  // Prevent metatable manipulation
    lua["getfenv"] = sol::lua_nil;
    lua["setfenv"] = sol::lua_nil;
    
    // Keep safe functions:
    // - print (will be overridden by API)
    // - type, tostring, tonumber
    // - pairs, ipairs, next
    // - select, unpack
    // - pcall, xpcall (for error handling)
    // - error, assert
    // - string.* (safe)
    // - table.* (safe)
    // - math.* (safe)
    // - coroutine.* (safe)
    
    // Setup execution limiter
    impl_->setup_execution_limiter(limits);
    
    // Provide a safe print that can be overridden
    lua["print"] = [](sol::variadic_args va) {
        // Default safe print does nothing in sandbox
        // Game API will override this
        (void)va;
    };
}

ScriptResult LuaState::execute(const std::string& script, const std::string& chunkName) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error, chunkName);
    
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::load(const std::string& script, const std::string& chunkName) {
    auto loadResult = impl_->lua.load(script, chunkName);
    
    if (!loadResult.valid()) {
        sol::error err = loadResult;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

bool LuaState::has_function(const std::string& funcName) const {
    sol::object obj = impl_->lua[funcName];
    return obj.is<sol::function>();
}

sol::state& LuaState::state() {
    return impl_->lua;
}

const sol::state& LuaState::state() const {
    return impl_->lua;
}

lua_State* LuaState::lua_state() {
    return impl_->lua.lua_state();
}

std::size_t LuaState::memory_used() const {
    if (impl_->memTracker) {
        return impl_->memTracker->allocated.load();
    }
    // Fallback: use Lua's internal count
    return static_cast<std::size_t>(lua_gc(impl_->lua.lua_state(), LUA_GCCOUNT, 0)) * 1024 +
           static_cast<std::size_t>(lua_gc(impl_->lua.lua_state(), LUA_GCCOUNTB, 0));
}

void LuaState::reset() {
    bool wasSandboxed = sandboxed_;
    ScriptLimits savedLimits = limits_;
    
    impl_ = std::make_unique<Impl>();
    impl_->init_default();
    
    sandboxed_ = false;
    if (wasSandboxed) {
        apply_sandbox(savedLimits);
    }
}

std::unique_ptr<LuaState> create_sandboxed_state(const ScriptLimits& limits) {
    auto state = std::make_unique<LuaState>();
    if (!state->init()) {
        return nullptr;
    }
    state->apply_sandbox(limits);
    return state;
}

std::unique_ptr<LuaState> create_engine_state() {
    auto state = std::make_unique<LuaState>();
    if (!state->init()) {
        return nullptr;
    }
    // Engine state: open all libraries including io, os, debug
    state->state().open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::table,
        sol::lib::math,
        sol::lib::utf8,
        sol::lib::io,
        sol::lib::os,
        sol::lib::debug,
        sol::lib::package
    );
    return state;
}

// Call overloads
ScriptResult LuaState::call(const std::string& funcName) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func();
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::call(const std::string& funcName, int arg1) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(arg1);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::call(const std::string& funcName, float arg1) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(arg1);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::call(const std::string& funcName, const std::string& arg1) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(arg1);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::call(const std::string& funcName, std::uint32_t arg1) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(arg1);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::call(const std::string& funcName, std::uint32_t arg1, int arg2, int arg3, int arg4) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(arg1, arg2, arg3, arg4);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

ScriptResult LuaState::call(const std::string& funcName, std::uint32_t arg1, int arg2, int arg3, int arg4, int arg5) {
    if (sandboxed_) {
        impl_->reset_execution_limiter();
    }
    
    sol::protected_function func = impl_->lua[funcName];
    if (!func.valid()) {
        return ScriptResult::fail("function '" + funcName + "' not found");
    }
    
    auto result = func(arg1, arg2, arg3, arg4, arg5);
    if (!result.valid()) {
        sol::error err = result;
        return ScriptResult::fail(err.what());
    }
    
    return ScriptResult::ok();
}

// Set global overloads
void LuaState::set_global(const std::string& name, int value) {
    impl_->lua[name] = value;
}

void LuaState::set_global(const std::string& name, float value) {
    impl_->lua[name] = value;
}

void LuaState::set_global(const std::string& name, double value) {
    impl_->lua[name] = value;
}

void LuaState::set_global(const std::string& name, bool value) {
    impl_->lua[name] = value;
}

void LuaState::set_global(const std::string& name, const std::string& value) {
    impl_->lua[name] = value;
}

// Get global overloads
std::optional<int> LuaState::get_global_int(const std::string& name) const {
    sol::object obj = impl_->lua[name];
    if (obj.is<int>()) {
        return obj.as<int>();
    }
    return std::nullopt;
}

std::optional<float> LuaState::get_global_float(const std::string& name) const {
    sol::object obj = impl_->lua[name];
    if (obj.is<float>()) {
        return obj.as<float>();
    }
    return std::nullopt;
}

std::optional<double> LuaState::get_global_double(const std::string& name) const {
    sol::object obj = impl_->lua[name];
    if (obj.is<double>()) {
        return obj.as<double>();
    }
    return std::nullopt;
}

std::optional<bool> LuaState::get_global_bool(const std::string& name) const {
    sol::object obj = impl_->lua[name];
    if (obj.is<bool>()) {
        return obj.as<bool>();
    }
    return std::nullopt;
}

std::optional<std::string> LuaState::get_global_string(const std::string& name) const {
    sol::object obj = impl_->lua[name];
    if (obj.is<std::string>()) {
        return obj.as<std::string>();
    }
    return std::nullopt;
}

} // namespace engine::scripting
