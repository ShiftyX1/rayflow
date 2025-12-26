#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace shared::scripting {

// Script metadata stored in .rfmap files
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

// Common script event types for maps
enum class MapEvent : std::uint8_t {
    // Player events
    PlayerJoin = 0,
    PlayerLeave,
    PlayerSpawn,
    PlayerDeath,
    PlayerRespawn,
    
    // Block events
    BlockBreak,
    BlockPlace,
    BlockInteract,
    
    // Game events
    RoundStart,
    RoundEnd,
    MatchStart,
    MatchEnd,
    
    // Timer events
    TimerTick,
    TimerComplete,
    
    // Custom events
    Custom,
    
    Count
};

// Get string name for map event
inline const char* map_event_name(MapEvent event) {
    switch (event) {
        case MapEvent::PlayerJoin: return "on_player_join";
        case MapEvent::PlayerLeave: return "on_player_leave";
        case MapEvent::PlayerSpawn: return "on_player_spawn";
        case MapEvent::PlayerDeath: return "on_player_death";
        case MapEvent::PlayerRespawn: return "on_player_respawn";
        case MapEvent::BlockBreak: return "on_block_break";
        case MapEvent::BlockPlace: return "on_block_place";
        case MapEvent::BlockInteract: return "on_block_interact";
        case MapEvent::RoundStart: return "on_round_start";
        case MapEvent::RoundEnd: return "on_round_end";
        case MapEvent::MatchStart: return "on_match_start";
        case MapEvent::MatchEnd: return "on_match_end";
        case MapEvent::TimerTick: return "on_timer_tick";
        case MapEvent::TimerComplete: return "on_timer_complete";
        case MapEvent::Custom: return "on_custom";
        default: return "unknown";
    }
}

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

} // namespace shared::scripting

