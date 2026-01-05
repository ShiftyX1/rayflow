#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace engine::scripting {

// Script metadata stored in map files (.rfmap, etc.)
struct MapScriptData {
    // Main script content (entry point)
    std::string mainScript;
    
    // Additional module scripts (name -> content)
    struct Module {
        std::string name;
        std::string content;
    };
    std::vector<Module> modules;
    
    // Script version (for compatibility)
    std::uint32_t version{1};
    
    // Total size in bytes
    std::size_t total_size() const {
        std::size_t size = mainScript.size();
        for (const auto& mod : modules) {
            size += mod.name.size() + mod.content.size();
        }
        return size;
    }
    
    bool empty() const {
        return mainScript.empty() && modules.empty();
    }
};

// UI script data for XML documents
struct UIScriptData {
    // Inline script content
    std::string inlineScript;
    
    // External script path (relative to ui/ directory)
    std::string externalPath;
    
    // Script associated with specific element IDs
    struct ElementScript {
        std::string elementId;
        std::string eventName;  // "click", "hover", "change", etc.
        std::string handler;    // Function name or inline code
    };
    std::vector<ElementScript> elementScripts;
    
    bool empty() const {
        return inlineScript.empty() && externalPath.empty() && elementScripts.empty();
    }
};

// Common UI event types
enum class UIEvent : std::uint8_t {
    Click = 0,
    Hover,
    HoverEnd,
    Focus,
    Blur,
    Change,
    Submit,
    KeyPress,
    Load,
    Unload,
    
    Count
};

inline const char* ui_event_name(UIEvent event) {
    switch (event) {
        case UIEvent::Click: return "on_click";
        case UIEvent::Hover: return "on_hover";
        case UIEvent::HoverEnd: return "on_hover_end";
        case UIEvent::Focus: return "on_focus";
        case UIEvent::Blur: return "on_blur";
        case UIEvent::Change: return "on_change";
        case UIEvent::Submit: return "on_submit";
        case UIEvent::KeyPress: return "on_key_press";
        case UIEvent::Load: return "on_load";
        case UIEvent::Unload: return "on_unload";
        default: return "unknown";
    }
}

} // namespace engine::scripting
