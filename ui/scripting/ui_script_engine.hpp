#pragma once

#include "../../shared/scripting/lua_state.hpp"
#include "../../shared/scripting/sandbox.hpp"
#include "../../shared/scripting/script_utils.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui::scripting {

// UI command from script to be executed by UI system
struct UICommand {
    enum class Type : std::uint8_t {
        None = 0,
        Show,           // Show element by ID
        Hide,           // Hide element by ID
        SetText,        // Set text content
        SetStyle,       // Set CSS property
        AddClass,       // Add CSS class
        RemoveClass,    // Remove CSS class
        PlaySound,      // Play a sound effect
        Animate,        // Start animation
    };
    
    Type type{Type::None};
    std::string elementId;
    std::string stringParam;
    std::string stringParam2;
    float floatParams[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

// Client-side UI script engine
class UIScriptEngine {
public:
    UIScriptEngine();
    ~UIScriptEngine();
    
    // Non-copyable
    UIScriptEngine(const UIScriptEngine&) = delete;
    UIScriptEngine& operator=(const UIScriptEngine&) = delete;
    
    // Initialize the engine
    bool init();
    
    // Load inline script from XML
    shared::scripting::ScriptResult load_script(const std::string& script, const std::string& name = "ui_script");
    
    // Unload current scripts
    void unload();
    
    // Check if scripts are loaded
    bool has_scripts() const { return scriptsLoaded_; }
    
    // Update (for animations, etc.)
    void update(float deltaTime);
    
    // Get and clear pending commands
    std::vector<UICommand> take_commands();
    
    // Event triggers
    void on_click(const std::string& elementId);
    void on_hover(const std::string& elementId, bool hovered);
    void on_focus(const std::string& elementId, bool focused);
    void on_change(const std::string& elementId, const std::string& value);
    void on_load();
    void on_unload();
    
    // Register element event handler from script
    void register_handler(const std::string& elementId, const std::string& eventName, const std::string& handlerFunc);
    
    // Get last error message
    const std::string& last_error() const { return lastError_; }
    
    // Set logging callback for script print() calls
    void set_log_callback(std::function<void(const std::string&)> callback);

private:
    void setup_ui_api();
    void call_handler(const std::string& elementId, const std::string& eventName);
    void call_handler(const std::string& elementId, const std::string& eventName, const std::string& arg);
    
    // Queue a command from script
    void queue_command(UICommand cmd);
    
    std::unique_ptr<shared::scripting::LuaState> lua_;
    
    bool scriptsLoaded_{false};
    std::string lastError_;
    
    std::vector<UICommand> pendingCommands_;
    
    // Event handlers: elementId -> eventName -> handlerFunction
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> handlers_;
    
    std::function<void(const std::string&)> logCallback_;
};

} // namespace ui::scripting

