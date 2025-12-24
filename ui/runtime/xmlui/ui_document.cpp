#include "ui_document.hpp"

#include <algorithm>
#include <cstdio>

#include <tinyxml2.h>

#include "../ui_view_model.hpp"
#include "../../../client/core/resources.hpp"

namespace ui::xmlui {

static std::string read_file_to_string(const char* path) {
    // Use VFS-aware resource loading.
    return resources::load_text(path);
}

static Rectangle anchor_rect(const UIStyle& style, int content_w, int content_h, int screen_w, int screen_h) {
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

static Color to_raylib_color(const UIColor& c) {
    return Color{c.r, c.g, c.b, c.a};
}

UIDocument::Node UIDocument::parse_node_rec(tinyxml2::XMLElement* el) {
    Node n;
    n.type = el->Name() ? el->Name() : "";

    if (const char* id = el->Attribute("id")) n.id = id;
    if (const char* cls = el->Attribute("class")) n.class_name = cls;

    // Element-specific attributes
    if (const char* full = el->Attribute("full")) n.full = full;
    if (const char* half = el->Attribute("half")) n.half = half;
    if (const char* empty = el->Attribute("empty")) n.empty = empty;
    if (const char* action = el->Attribute("action")) n.action = action;

    // Text content
    if (const char* txt = el->GetText()) {
        n.text = txt;
    }

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
        TraceLog(LOG_ERROR, "[ui] Failed to read CSS file: %s", css_path);
        return false;
    }
    const CSSParseResult cssRes = parse_css_lite(css);
    if (!cssRes.ok()) {
        TraceLog(LOG_ERROR, "[ui] CSS parse error in %s: %s", css_path, cssRes.error.c_str());
        return false;
    }
    rules_ = cssRes.rules;
    TraceLog(LOG_DEBUG, "[ui] Loaded %zu CSS rules from: %s", rules_.size(), css_path);

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xml_path) != tinyxml2::XML_SUCCESS) {
        TraceLog(LOG_ERROR, "[ui] Failed to load XML file: %s (error: %s)", xml_path, doc.ErrorStr());
        return false;
    }

    auto* rootEl = doc.FirstChildElement("UI");
    if (!rootEl) {
        TraceLog(LOG_ERROR, "[ui] XML file %s has no <UI> root element", xml_path);
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

    for (auto& [_, font] : font_cache_) {
        if (font.texture.id != 0) {
            UnloadFont(font);
        }
    }
    font_cache_.clear();
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
    ref.tex = resources::load_texture(path);
    texture_cache_.emplace(path, ref);
    return ref.tex;
}

Font UIDocument::load_font_cached(int size) {
    auto it = font_cache_.find(size);
    if (it != font_cache_.end()) {
        return it->second;
    }

    // Use default raylib font scaled (or load a custom font)
    Font f = GetFontDefault();
    font_cache_.emplace(size, f);
    return f;
}

int UIDocument::measure_content_width(const Node& node) {
    if (node.style.width.has_value()) {
        return *node.style.width;
    }

    // For Text, measure text width
    if (node.type == "Text" && !node.text.empty()) {
        const int fontSize = node.style.font_size;
        return MeasureText(node.text.c_str(), fontSize);
    }

    // For Button, measure text width + padding
    if (node.type == "Button" && !node.text.empty()) {
        const int fontSize = node.style.font_size;
        return MeasureText(node.text.c_str(), fontSize) + node.style.padding.left + node.style.padding.right;
    }

    // For containers (Panel, Row, Column), sum children
    if (node.type == "Row") {
        int total = 0;
        for (const auto& c : node.children) {
            total += measure_content_width(c);
        }
        total += static_cast<int>(node.children.size() > 1 ? (node.children.size() - 1) * node.style.gap : 0);
        return total + node.style.padding.left + node.style.padding.right;
    }

    if (node.type == "Panel" || node.type == "Column") {
        int maxW = 0;
        for (const auto& c : node.children) {
            maxW = std::max(maxW, measure_content_width(c));
        }
        return maxW + node.style.padding.left + node.style.padding.right;
    }

    return 100; // Default fallback
}

int UIDocument::measure_content_height(const Node& node) {
    if (node.style.height.has_value()) {
        return *node.style.height;
    }

    // For Text
    if (node.type == "Text") {
        return node.style.font_size;
    }

    // For Button
    if (node.type == "Button") {
        return node.style.font_size + node.style.padding.top + node.style.padding.bottom;
    }

    // For Column, sum children
    if (node.type == "Column" || node.type == "Panel") {
        int total = 0;
        for (const auto& c : node.children) {
            total += measure_content_height(c);
        }
        total += static_cast<int>(node.children.size() > 1 ? (node.children.size() - 1) * node.style.gap : 0);
        return total + node.style.padding.top + node.style.padding.bottom;
    }

    // For Row
    if (node.type == "Row") {
        int maxH = 0;
        for (const auto& c : node.children) {
            maxH = std::max(maxH, measure_content_height(c));
        }
        return maxH + node.style.padding.top + node.style.padding.bottom;
    }

    return 50; // Default fallback
}

void UIDocument::layout(Node& node, Rectangle available, const UIViewModel& vm) {
    // Determine actual dimensions
    int w = node.style.width.has_value() ? *node.style.width : measure_content_width(node);
    int h = node.style.height.has_value() ? *node.style.height : measure_content_height(node);

    // Handle grow: expand to available size if grow is set
    if (node.style.grow) {
        w = static_cast<int>(available.width);
        h = static_cast<int>(available.height);
    }

    // Top-level nodes use anchor positioning
    if (&node != &root_ && node.style.anchor != UIAnchor::TopLeft) {
        node.computed_rect = anchor_rect(node.style, w, h, vm.screen_width, vm.screen_height);
    } else {
        // Position at available rect origin
        node.computed_rect = Rectangle{available.x, available.y, static_cast<float>(w), static_cast<float>(h)};
    }

    // Layout children based on direction
    if (!node.children.empty()) {
        const float px = static_cast<float>(node.style.padding.left);
        const float py = static_cast<float>(node.style.padding.top);
        const float pw = node.computed_rect.width - node.style.padding.left - node.style.padding.right;
        const float ph = node.computed_rect.height - node.style.padding.top - node.style.padding.bottom;

        float curX = node.computed_rect.x + px;
        float curY = node.computed_rect.y + py;

        for (auto& child : node.children) {
            Rectangle childAvail{curX, curY, pw, ph};
            layout(child, childAvail, vm);

            if (node.style.direction == UIDirection::Column) {
                curY += child.computed_rect.height + node.style.gap;
            } else {
                curX += child.computed_rect.width + node.style.gap;
            }
        }
    }
}

bool UIDocument::update_node_rec(Node& node, Vector2 mouse_pos, bool mouse_down, bool mouse_pressed) {
    bool captured = false;

    // Check if mouse is over this node
    const bool over = CheckCollisionPointRec(mouse_pos, node.computed_rect);
    node.hovered = over;

    if (node.type == "Button" && over) {
        captured = true;
        node.pressed = mouse_down;

        if (mouse_pressed && on_click_) {
            const std::string click_id = node.action.empty() ? node.id : node.action;
            if (!click_id.empty()) {
                on_click_(click_id);
            }
        }
    }

    // Recurse children
    for (auto& child : node.children) {
        if (update_node_rec(child, mouse_pos, mouse_down, mouse_pressed)) {
            captured = true;
        }
    }

    return captured;
}

bool UIDocument::update(const UIViewModel& vm, Vector2 mouse_pos, bool mouse_down, bool mouse_pressed) {
    if (!loaded_) {
        return false;
    }

    // Run layout pass
    Rectangle full_screen{0, 0, static_cast<float>(vm.screen_width), static_cast<float>(vm.screen_height)};
    for (auto& child : root_.children) {
        int w = child.style.width.has_value() ? *child.style.width : measure_content_width(child);
        int h = child.style.height.has_value() ? *child.style.height : measure_content_height(child);
        Rectangle positioned = anchor_rect(child.style, w, h, vm.screen_width, vm.screen_height);
        layout(child, positioned, vm);
    }

    // Update interaction state
    bool captured = false;
    for (auto& child : root_.children) {
        if (update_node_rec(child, mouse_pos, mouse_down, mouse_pressed)) {
            captured = true;
        }
    }

    return captured;
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
    if (!node.style.visible) {
        return;
    }

    if (node.type == "HealthBar") {
        render_health_bar(node, vm);
        return;
    }

    if (node.type == "Panel" || node.type == "Column" || node.type == "Row") {
        render_panel(node, vm);
        return;
    }

    if (node.type == "Text") {
        render_text(node, vm);
        return;
    }

    if (node.type == "Button") {
        render_button(node, vm);
        return;
    }

    if (node.type == "Spacer") {
        // Spacer does not render anything; it just takes up space.
        return;
    }

    // Unknown node type: render children only
    for (const auto& child : node.children) {
        render_node(child, vm);
    }
}

void UIDocument::render_panel(const Node& node, const UIViewModel& vm) {
    const Rectangle& r = node.computed_rect;

    // Background
    if (node.style.background_color.has_value()) {
        if (node.style.border_radius > 0) {
            DrawRectangleRounded(r, static_cast<float>(node.style.border_radius) / std::min(r.width, r.height), 8, to_raylib_color(*node.style.background_color));
        } else {
            DrawRectangleRec(r, to_raylib_color(*node.style.background_color));
        }
    }

    // Border
    if (node.style.border_width > 0) {
        if (node.style.border_radius > 0) {
            DrawRectangleRoundedLinesEx(r, static_cast<float>(node.style.border_radius) / std::min(r.width, r.height), 8, static_cast<float>(node.style.border_width), to_raylib_color(node.style.border_color));
        } else {
            DrawRectangleLinesEx(r, static_cast<float>(node.style.border_width), to_raylib_color(node.style.border_color));
        }
    }

    // Render children
    for (const auto& child : node.children) {
        render_node(child, vm);
    }
}

void UIDocument::render_text(const Node& node, const UIViewModel& vm) {
    (void)vm;

    if (node.text.empty()) {
        return;
    }

    const int fontSize = node.style.font_size;
    const Color color = to_raylib_color(node.style.color);

    DrawText(node.text.c_str(), static_cast<int>(node.computed_rect.x), static_cast<int>(node.computed_rect.y), fontSize, color);
}

void UIDocument::render_button(const Node& node, const UIViewModel& vm) {
    (void)vm;

    const Rectangle& r = node.computed_rect;

    // Button background with hover/pressed states
    UIColor bg = node.style.background_color.value_or(UIColor{60, 60, 60, 255});
    if (node.pressed) {
        bg.r = static_cast<unsigned char>(bg.r * 0.7f);
        bg.g = static_cast<unsigned char>(bg.g * 0.7f);
        bg.b = static_cast<unsigned char>(bg.b * 0.7f);
    } else if (node.hovered) {
        bg.r = static_cast<unsigned char>(std::min(255, bg.r + 30));
        bg.g = static_cast<unsigned char>(std::min(255, bg.g + 30));
        bg.b = static_cast<unsigned char>(std::min(255, bg.b + 30));
    }

    if (node.style.border_radius > 0) {
        DrawRectangleRounded(r, static_cast<float>(node.style.border_radius) / std::min(r.width, r.height), 8, to_raylib_color(bg));
    } else {
        DrawRectangleRec(r, to_raylib_color(bg));
    }

    // Border
    if (node.style.border_width > 0) {
        UIColor border_col = node.style.border_color;
        if (node.hovered) {
            border_col.r = static_cast<unsigned char>(std::min(255, border_col.r + 50));
            border_col.g = static_cast<unsigned char>(std::min(255, border_col.g + 50));
            border_col.b = static_cast<unsigned char>(std::min(255, border_col.b + 50));
        }
        if (node.style.border_radius > 0) {
            DrawRectangleRoundedLinesEx(r, static_cast<float>(node.style.border_radius) / std::min(r.width, r.height), 8, static_cast<float>(node.style.border_width), to_raylib_color(border_col));
        } else {
            DrawRectangleLinesEx(r, static_cast<float>(node.style.border_width), to_raylib_color(border_col));
        }
    }

    // Text centered
    if (!node.text.empty()) {
        const int fontSize = node.style.font_size;
        const int textW = MeasureText(node.text.c_str(), fontSize);
        const int tx = static_cast<int>(r.x + (r.width - textW) / 2);
        const int ty = static_cast<int>(r.y + (r.height - fontSize) / 2);
        DrawText(node.text.c_str(), tx, ty, fontSize, to_raylib_color(node.style.color));
    }
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
