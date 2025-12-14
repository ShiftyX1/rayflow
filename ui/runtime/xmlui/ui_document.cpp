#include "ui_document.hpp"

#include <cstdio>

#include <tinyxml2.h>

#include "../ui_view_model.hpp"

namespace ui::xmlui {

static std::string read_file_to_string(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return {};
    }
    std::fseek(f, 0, SEEK_END);
    long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        std::fclose(f);
        return {};
    }
    std::string data;
    data.resize(static_cast<size_t>(len));
    (void)std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    return data;
}

static Rectangle anchor_rect(const UIStyle& style, int content_w, int content_h, int screen_w, int screen_h) {
    // Apply margin as offset from edges for anchored HUD elements.
    const int mL = style.margin.left;
    const int mT = style.margin.top;
    const int mR = style.margin.right;
    const int mB = style.margin.bottom;

    int x = 0;
    int y = 0;

    switch (style.anchor) {
        case UIAnchor::TopLeft:
            x = mL;
            y = mT;
            break;
        case UIAnchor::Top:
            x = (screen_w - content_w) / 2;
            y = mT;
            break;
        case UIAnchor::TopRight:
            x = screen_w - content_w - mR;
            y = mT;
            break;
        case UIAnchor::Left:
            x = mL;
            y = (screen_h - content_h) / 2;
            break;
        case UIAnchor::Center:
            x = (screen_w - content_w) / 2;
            y = (screen_h - content_h) / 2;
            break;
        case UIAnchor::Right:
            x = screen_w - content_w - mR;
            y = (screen_h - content_h) / 2;
            break;
        case UIAnchor::BottomLeft:
            x = mL;
            y = screen_h - content_h - mB;
            break;
        case UIAnchor::Bottom:
            x = (screen_w - content_w) / 2;
            y = screen_h - content_h - mB;
            break;
        case UIAnchor::BottomRight:
            x = screen_w - content_w - mR;
            y = screen_h - content_h - mB;
            break;
    }

    return Rectangle{static_cast<float>(x), static_cast<float>(y), static_cast<float>(content_w), static_cast<float>(content_h)};
}


UIDocument::Node UIDocument::parse_node_rec(tinyxml2::XMLElement* el) {
    Node n;
    n.type = el->Name() ? el->Name() : "";

    if (const char* id = el->Attribute("id")) n.id = id;
    if (const char* cls = el->Attribute("class")) n.class_name = cls;

    if (const char* full = el->Attribute("full")) n.full = full;
    if (const char* half = el->Attribute("half")) n.half = half;
    if (const char* empty = el->Attribute("empty")) n.empty = empty;

    for (auto* child = el->FirstChildElement(); child; child = child->NextSiblingElement()) {
        n.children.push_back(parse_node_rec(child));
    }

    return n;
}

void UIDocument::apply_styles_rec(Node& n) {
    n.style = compute_style(rules_, n.type, n.id, n.class_name);
    for (auto& c : n.children) {
        apply_styles_rec(c);
    }
}

bool UIDocument::load_from_files(const char* xml_path, const char* css_path) {
    unload();

    const std::string css = read_file_to_string(css_path);
    if (css.empty()) {
        return false;
    }
    const CSSParseResult cssRes = parse_css_lite(css);
    if (!cssRes.ok()) {
        return false;
    }
    rules_ = cssRes.rules;

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xml_path) != tinyxml2::XML_SUCCESS) {
        return false;
    }

    auto* rootEl = doc.FirstChildElement("UI");
    if (!rootEl) {
        return false;
    }

    root_.type = "UI";
    root_.id.clear();
    root_.class_name.clear();
    root_.children.clear();

    for (auto* child = rootEl->FirstChildElement(); child; child = child->NextSiblingElement()) {
        root_.children.push_back(parse_node_rec(child));
    }

    apply_styles_rec(root_);
    loaded_ = true;
    return true;
}

void UIDocument::unload() {
    loaded_ = false;
    root_ = {};
    rules_.clear();

    for (auto& [_, ref] : texture_cache_) {
        if (ref.tex.id != 0) {
            UnloadTexture(ref.tex);
        }
    }
    texture_cache_.clear();
}

Texture2D UIDocument::load_texture_cached(const std::string& path) {
    if (path.empty()) {
        return Texture2D{};
    }
    auto it = texture_cache_.find(path);
    if (it != texture_cache_.end()) {
        return it->second.tex;
    }

    TextureRef ref;
    ref.tex = LoadTexture(path.c_str());
    texture_cache_.emplace(path, ref);
    return ref.tex;
}

void UIDocument::render(const UIViewModel& vm) {
    if (!loaded_) {
        return;
    }

    for (const auto& child : root_.children) {
        render_node(child, vm);
    }
}

void UIDocument::render_node(const Node& node, const UIViewModel& vm) {
    if (node.type == "HealthBar") {
        render_health_bar(node, vm);
        return;
    }

    // For MVP we only need HealthBar. Other nodes can be added next.
    (void)node;
    (void)vm;
}

void UIDocument::render_health_bar(const Node& node, const UIViewModel& vm) {
    const Texture2D full = load_texture_cached(node.full);
    const Texture2D half = load_texture_cached(node.half);
    const Texture2D empty = load_texture_cached(node.empty);

    if (full.id == 0 || half.id == 0 || empty.id == 0) {
        return;
    }

    const int maxHealth = (vm.player.max_health <= 0) ? 20 : vm.player.max_health;
    int health = vm.player.health;
    if (health < 0) health = 0;
    if (health > maxHealth) health = maxHealth;

    const int hearts = (maxHealth + 1) / 2;
    const int fullHearts = health / 2;
    const bool hasHalf = (health % 2) != 0;

    const int heartW = node.style.width.has_value() ? *node.style.width : full.width;
    const int heartH = node.style.height.has_value() ? *node.style.height : full.height;

    const int gap = node.style.gap;
    const int contentW = hearts * heartW + (hearts > 0 ? (hearts - 1) * gap : 0);
    const int contentH = heartH;

    const Rectangle r = anchor_rect(node.style, contentW, contentH, vm.screen_width, vm.screen_height);

    for (int i = 0; i < hearts; i++) {
        const int x = static_cast<int>(r.x) + i * (heartW + gap);
        const int y = static_cast<int>(r.y);

        Texture2D tex = empty;
        if (i < fullHearts) {
            tex = full;
        } else if (i == fullHearts && hasHalf) {
            tex = half;
        }

        const Rectangle src{0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
        const Rectangle dst{static_cast<float>(x), static_cast<float>(y), static_cast<float>(heartW), static_cast<float>(heartH)};
        DrawTexturePro(tex, src, dst, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    }
}

} // namespace ui::xmlui
