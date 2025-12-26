#include "ui_script_engine.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <sstream>

namespace ui::scripting {

UIScriptEngine::UIScriptEngine() = default;
UIScriptEngine::~UIScriptEngine() = default;

bool UIScriptEngine::init() {
    // Create sandboxed Lua state with UI-specific limits
    auto config = shared::scripting::SandboxConfig::default_for_ui();
    
    // Set up print handler
    config.printHandler = [this](const std::string& msg) {
        if (logCallback_) {
            logCallback_(msg);
        }
    };
    
    lua_ = shared::scripting::Sandbox::create(config);
    if (!lua_) {
        lastError_ = "Failed to create sandboxed Lua state for UI";
        return false;
    }
    
    setup_ui_api();
    return true;
}

void UIScriptEngine::setup_ui_api() {
    if (!lua_) return;
    
    auto& lua = lua_->state();
    
    // Create 'ui' namespace
    auto ui = lua.create_named_table("ui");
    
    // Element manipulation
    ui["show"] = [this](const std::string& id) {
        UICommand cmd;
        cmd.type = UICommand::Type::Show;
        cmd.elementId = id;
        queue_command(std::move(cmd));
    };
    
    ui["hide"] = [this](const std::string& id) {
        UICommand cmd;
        cmd.type = UICommand::Type::Hide;
        cmd.elementId = id;
        queue_command(std::move(cmd));
    };
    
    ui["set_text"] = [this](const std::string& id, const std::string& text) {
        UICommand cmd;
        cmd.type = UICommand::Type::SetText;
        cmd.elementId = id;
        cmd.stringParam = text;
        queue_command(std::move(cmd));
    };
    
    ui["set_style"] = [this](const std::string& id, const std::string& prop, const std::string& value) {
        UICommand cmd;
        cmd.type = UICommand::Type::SetStyle;
        cmd.elementId = id;
        cmd.stringParam = prop;
        cmd.stringParam2 = value;
        queue_command(std::move(cmd));
    };
    
    ui["add_class"] = [this](const std::string& id, const std::string& className) {
        UICommand cmd;
        cmd.type = UICommand::Type::AddClass;
        cmd.elementId = id;
        cmd.stringParam = className;
        queue_command(std::move(cmd));
    };
    
    ui["remove_class"] = [this](const std::string& id, const std::string& className) {
        UICommand cmd;
        cmd.type = UICommand::Type::RemoveClass;
        cmd.elementId = id;
        cmd.stringParam = className;
        queue_command(std::move(cmd));
    };
    
    // Sound API (local only)
    ui["play_sound"] = [this](const std::string& soundName) {
        UICommand cmd;
        cmd.type = UICommand::Type::PlaySound;
        cmd.stringParam = soundName;
        queue_command(std::move(cmd));
    };
    
    // Animation API
    ui["animate"] = [this](const std::string& id, const std::string& animName, float duration) {
        UICommand cmd;
        cmd.type = UICommand::Type::Animate;
        cmd.elementId = id;
        cmd.stringParam = animName;
        cmd.floatParams[0] = duration;
        queue_command(std::move(cmd));
    };
    
    // Event handler registration
    ui["on"] = [this](const std::string& elementId, const std::string& eventName, sol::function handler) {
        // Generate a unique function name and store the handler
        std::string funcName = "__ui_handler_" + elementId + "_" + eventName;
        lua_->state()[funcName] = handler;
        register_handler(elementId, eventName, funcName);
    };
    
    // Override print with our log function
    lua["print"] = [this](sol::variadic_args va, sol::this_state ts) {
        sol::state_view luaView(ts);
        std::ostringstream oss;
        
        bool first = true;
        for (auto v : va) {
            if (!first) oss << "\t";
            first = false;
            
            sol::protected_function tostring = luaView["tostring"];
            if (tostring.valid()) {
                auto result = tostring(v);
                if (result.valid()) {
                    oss << result.get<std::string>();
                }
            }
        }
        
        if (logCallback_) {
            logCallback_("[ui] " + oss.str());
        }
    };
}

shared::scripting::ScriptResult UIScriptEngine::load_script(const std::string& script, const std::string& name) {
    if (!lua_) {
        return shared::scripting::ScriptResult::fail("Engine not initialized");
    }
    
    // Validate script
    auto validation = shared::scripting::Sandbox::validate_script(script);
    if (!validation.valid) {
        std::string errors;
        for (const auto& err : validation.errors) {
            if (!errors.empty()) errors += "; ";
            errors += err;
        }
        lastError_ = "Script validation failed: " + errors;
        return shared::scripting::ScriptResult::fail(lastError_);
    }
    
    // Execute script
    auto result = lua_->execute(script, name);
    if (!result) {
        lastError_ = "Failed to load script: " + result.error;
        return shared::scripting::ScriptResult::fail(lastError_);
    }
    
    scriptsLoaded_ = true;
    
    // Call on_load if present
    if (lua_->has_function("on_load")) {
        lua_->call("on_load");
    }
    
    return shared::scripting::ScriptResult::ok();
}

void UIScriptEngine::unload() {
    if (!lua_) return;
    
    if (scriptsLoaded_ && lua_->has_function("on_unload")) {
        lua_->call("on_unload");
    }
    
    scriptsLoaded_ = false;
    handlers_.clear();
    pendingCommands_.clear();
    
    lua_->reset();
    setup_ui_api();
}

void UIScriptEngine::update(float deltaTime) {
    if (!scriptsLoaded_) return;
    
    // Call update hook if present
    if (lua_->has_function("on_update")) {
        lua_->call("on_update", deltaTime);
    }
}

std::vector<UICommand> UIScriptEngine::take_commands() {
    std::vector<UICommand> result;
    result.swap(pendingCommands_);
    return result;
}

void UIScriptEngine::queue_command(UICommand cmd) {
    pendingCommands_.push_back(std::move(cmd));
}

void UIScriptEngine::register_handler(const std::string& elementId, 
                                       const std::string& eventName, 
                                       const std::string& handlerFunc) {
    handlers_[elementId][eventName] = handlerFunc;
}

void UIScriptEngine::call_handler(const std::string& elementId, const std::string& eventName) {
    if (!scriptsLoaded_ || !lua_) return;
    
    auto elemIt = handlers_.find(elementId);
    if (elemIt == handlers_.end()) return;
    
    auto eventIt = elemIt->second.find(eventName);
    if (eventIt == elemIt->second.end()) return;
    
    const auto& funcName = eventIt->second;
    if (lua_->has_function(funcName)) {
        auto result = lua_->call(funcName);
        if (!result) {
            lastError_ = "Handler error: " + result.error;
            if (logCallback_) {
                logCallback_("[ui error] " + lastError_);
            }
        }
    }
}

void UIScriptEngine::call_handler(const std::string& elementId, 
                                   const std::string& eventName, 
                                   const std::string& arg) {
    if (!scriptsLoaded_ || !lua_) return;
    
    auto elemIt = handlers_.find(elementId);
    if (elemIt == handlers_.end()) return;
    
    auto eventIt = elemIt->second.find(eventName);
    if (eventIt == elemIt->second.end()) return;
    
    const auto& funcName = eventIt->second;
    if (lua_->has_function(funcName)) {
        lua_->state()[funcName](arg);
    }
}

void UIScriptEngine::on_click(const std::string& elementId) {
    call_handler(elementId, "click");
    
    // Also call global on_click if present
    if (lua_->has_function("on_click")) {
        lua_->state()["on_click"](elementId);
    }
}

void UIScriptEngine::on_hover(const std::string& elementId, bool hovered) {
    call_handler(elementId, hovered ? "hover" : "hover_end");
}

void UIScriptEngine::on_focus(const std::string& elementId, bool focused) {
    call_handler(elementId, focused ? "focus" : "blur");
}

void UIScriptEngine::on_change(const std::string& elementId, const std::string& value) {
    call_handler(elementId, "change", value);
}

void UIScriptEngine::on_load() {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_load")) {
        lua_->call("on_load");
    }
}

void UIScriptEngine::on_unload() {
    if (!scriptsLoaded_ || !lua_) return;
    
    if (lua_->has_function("on_unload")) {
        lua_->call("on_unload");
    }
}

void UIScriptEngine::set_log_callback(std::function<void(const std::string&)> callback) {
    logCallback_ = std::move(callback);
}

} // namespace ui::scripting

