#pragma once

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

namespace ui::xmlui {

class UIDocument {
public:
    bool load_from_files(const char* xml_path, const char* css_path);
    void unload();

    void render(const UIViewModel& vm);

private:
    struct Node {
        std::string type;
        std::string id;
        std::string class_name;

        // element-specific attributes
        std::string full;
        std::string half;
        std::string empty;

        UIStyle style{};
        std::vector<Node> children;
    };

    struct TextureRef {
        Texture2D tex{};
    };

    Texture2D load_texture_cached(const std::string& path);

    Node parse_node_rec(tinyxml2::XMLElement* el);
    void apply_styles_rec(Node& n);

    void render_node(const Node& node, const UIViewModel& vm);
    void render_health_bar(const Node& node, const UIViewModel& vm);

    std::vector<CSSRule> rules_{};
    Node root_{};

    bool loaded_{false};
    std::unordered_map<std::string, TextureRef> texture_cache_{};
};

} // namespace ui::xmlui
