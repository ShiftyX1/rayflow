#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <raylib.h>

#include "css_lite.hpp"

namespace ui {
struct UIViewModel;
}

namespace tinyxml2 {
class XMLElement;
}

namespace ui::scripting {
class UIScriptEngine;
}

namespace ui::xmlui {

using OnClickCallback = std::function<void(const std::string& id)>;

class UIDocument {
public:
    UIDocument();
    ~UIDocument();
    
    bool load_from_files(const char* xml_path, const char* css_path);
    void unload();

    bool update(const UIViewModel& vm, Vector2 mouse_pos, bool mouse_down, bool mouse_pressed);

    void render(const UIViewModel& vm);

    void set_on_click(OnClickCallback cb) { on_click_ = std::move(cb); }
    
    // Check if document has scripts
    bool has_scripts() const { return scriptEngine_ != nullptr; }
    
    // Set logging callback for scripts
    void set_script_log_callback(std::function<void(const std::string&)> callback);

private:
    struct Node {
        std::string type;
        std::string id;
        std::string class_name;

        std::string text;

        std::string full;
        std::string half;
        std::string empty;

        std::string action;

        UIStyle style{};
        std::vector<Node> children;

        Rectangle computed_rect{0, 0, 0, 0};

        bool hovered{false};
        bool pressed{false};
        bool focused{false};
    };

    struct TextureRef {
        Texture2D tex{};
    };

    Texture2D load_texture_cached(const std::string& path);
    Font load_font_cached(int size);

    Node parse_node_rec(tinyxml2::XMLElement* el);
    void apply_styles_rec(Node& n);

    void layout(Node& node, Rectangle available, const UIViewModel& vm);
    int measure_content_width(const Node& node);
    int measure_content_height(const Node& node);

    bool update_node_rec(Node& node, Vector2 mouse_pos, bool mouse_down, bool mouse_pressed);

    void render_node(const Node& node, const UIViewModel& vm);
    void render_health_bar(const Node& node, const UIViewModel& vm);
    void render_panel(const Node& node, const UIViewModel& vm);
    void render_text(const Node& node, const UIViewModel& vm);
    void render_button(const Node& node, const UIViewModel& vm);

    std::vector<CSSRule> rules_{};
    Node root_{};

    bool loaded_{false};
    std::unordered_map<std::string, TextureRef> texture_cache_{};
    std::unordered_map<int, Font> font_cache_{};

    OnClickCallback on_click_{};
    
    // Lua scripting
    std::unique_ptr<scripting::UIScriptEngine> scriptEngine_{};
    std::function<void(const std::string&)> scriptLogCallback_{};
    
    void parse_script_element(tinyxml2::XMLElement* el);
    void process_script_commands();
    void notify_script_click(const std::string& elementId);
    void notify_script_hover(const std::string& elementId, bool hovered);
};

} // namespace ui::xmlui
