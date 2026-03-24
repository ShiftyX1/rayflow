#include "ui_document.hpp"

#include <algorithm>
#include <cstdio>

#include <tinyxml2.h>

#include "../ui_view_model.hpp"
#include "engine/client/core/resources.hpp"
#include "engine/core/logging.hpp"
#include "engine/core/math_types.hpp"
#include "engine/renderer/batch_2d.hpp"
#include "engine/renderer/gl_font.hpp"
#include "engine/ui/scripting/ui_script_engine.hpp"
#include "engine/vfs/vfs.hpp"

namespace ui::xmlui {

UIDocument::UIDocument() = default;
UIDocument::~UIDocument() = default;

static std::string read_file_to_string(const char* path) {
    // Use VFS-aware resource loading.
    return resources::load_text(path);
}

static rf::Rect anchor_rect(const UIStyle& style, int content_w, int content_h, int screen_w, int screen_h) {
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

    return rf::Rect{static_cast<float>(x), static_cast<float>(y), static_cast<float>(content_w), static_cast<float>(content_h)};
}

static rf::Color to_rf_color(const UIColor& c) {
    return rf::Color{c.r, c.g, c.b, c.a};
}

static rf::Color apply_opacity(const UIColor& c, float opacity) {
    return rf::Color{
        c.r,
        c.g,
        c.b,
        static_cast<unsigned char>(c.a * opacity)
    };
}

static void draw_box_shadow(const rf::Rect& r, const UIStyle& style) {
    if (!style.has_shadow) return;
    
    const float blur = static_cast<float>(style.shadow_blur);
    const float offsetX = static_cast<float>(style.shadow_offset_x);
    const float offsetY = static_cast<float>(style.shadow_offset_y);
    
    const int layers = std::max(1, std::min(10, style.shadow_blur / 2));
    for (int i = 0; i < layers; ++i) {
        const float alpha = (1.0f - (static_cast<float>(i) / layers)) * (style.shadow_color.a / 255.0f);
        const float spread = (static_cast<float>(i) / layers) * blur;
        
        rf::Rect shadowRect = {
            r.x + offsetX - spread,
            r.y + offsetY - spread,
            r.w + spread * 2,
            r.h + spread * 2
        };
        
        rf::Color shadowColor = {
            style.shadow_color.r,
            style.shadow_color.g,
            style.shadow_color.b,
            static_cast<unsigned char>(alpha * 255)
        };
        
        auto& batch = rf::Batch2D::instance();
        if (style.border_radius > 0) {
            batch.drawRectRounded(shadowRect.x, shadowRect.y, shadowRect.w, shadowRect.h,
                static_cast<float>(style.border_radius) / std::min(shadowRect.w, shadowRect.h),
                8, shadowColor);
        } else {
            batch.drawRect(shadowRect.x, shadowRect.y, shadowRect.w, shadowRect.h, shadowColor);
        }
    }
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

    // Load XML via VFS (supports both loose files and PAK archives)
    const std::string xml = read_file_to_string(xml_path);
    if (xml.empty()) {
        TraceLog(LOG_ERROR, "[ui] Failed to read XML file: %s", xml_path);
        return false;
    }

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
        TraceLog(LOG_ERROR, "[ui] Failed to parse XML file: %s (error: %s)", xml_path, doc.ErrorStr());
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
        const char* childName = child->Name();
        if (childName && std::string(childName) == "script") {
            // Parse script element
            parse_script_element(child);
        } else {
            root_.children.push_back(parse_node_rec(child));
        }
    }

    apply_styles_rec(root_);
    loaded_ = true;
    return true;
}

void UIDocument::unload() {
    loaded_ = false;
    root_ = {};
    rules_.clear();

    // Unload script engine
    if (scriptEngine_) {
        scriptEngine_->unload();
        scriptEngine_.reset();
    }

    for (auto& [_, ref] : texture_cache_) {
        ref.tex.reset();
    }
    texture_cache_.clear();

    for (auto& [_, font] : font_cache_) {
        font.reset();
    }
    font_cache_.clear();
}

rf::ITexture* UIDocument::load_texture_cached(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }
    auto it = texture_cache_.find(path);
    if (it != texture_cache_.end()) {
        return it->second.tex.get();
    }

    TextureRef ref;
    ref.tex = resources::load_texture(path);
    rf::ITexture* ptr = ref.tex.get();
    texture_cache_.emplace(path, std::move(ref));
    return ptr;
}

rf::GLFont* UIDocument::load_font_cached(int size) {
    auto it = font_cache_.find(size);
    if (it != font_cache_.end()) {
        return it->second.get();
    }

    auto font = std::make_unique<rf::GLFont>();
    // Try to use the default system font at the requested size
    rf::GLFont* def = rf::GLFont::defaultFont();
    if (def && def->isValid()) {
        // For now, all sizes share the default font (it scales via drawText size parameter)
        // To get per-size atlases, we'd bake a new atlas per size.
        // Just return default font for now.
        font_cache_.emplace(size, nullptr);
        return def;
    }
    font_cache_.emplace(size, std::move(font));
    return font_cache_[size].get();
}

int UIDocument::measure_content_width(const Node& node) {
    if (node.style.width.has_value()) {
        return *node.style.width;
    }

    // For Text, measure text width
    if (node.type == "Text" && !node.text.empty()) {
        const int fontSize = node.style.font_size;
        return static_cast<int>(rf::Batch2D::instance().measureText(node.text, static_cast<float>(fontSize)));
    }

    // For Button, measure text width + padding
    if (node.type == "Button" && !node.text.empty()) {
        const int fontSize = node.style.font_size;
        return static_cast<int>(rf::Batch2D::instance().measureText(node.text, static_cast<float>(fontSize))) + node.style.padding.left + node.style.padding.right;
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

void UIDocument::layout(Node& node, rf::Rect available, const UIViewModel& vm) {
    // Determine actual dimensions
    int w = node.style.width.has_value() ? *node.style.width : measure_content_width(node);
    int h = node.style.height.has_value() ? *node.style.height : measure_content_height(node);

    // Handle grow: expand to available size if grow is set
    if (node.style.grow) {
        w = static_cast<int>(available.w);
        h = static_cast<int>(available.h);
    }

    // Top-level nodes use anchor positioning
    if (&node != &root_ && node.style.anchor != UIAnchor::TopLeft) {
        node.computed_rect = anchor_rect(node.style, w, h, vm.screen_width, vm.screen_height);
    } else {
        // Position at available rect origin
        node.computed_rect = rf::Rect{available.x, available.y, static_cast<float>(w), static_cast<float>(h)};
    }

    // Layout children based on direction
    if (!node.children.empty()) {
        const float px = static_cast<float>(node.style.padding.left);
        const float py = static_cast<float>(node.style.padding.top);
        const float pw = node.computed_rect.w - node.style.padding.left - node.style.padding.right;
        const float ph = node.computed_rect.h - node.style.padding.top - node.style.padding.bottom;

        // Calculate total children size for justify-content
        float totalChildrenSize = 0;
        for (auto& child : node.children) {
            if (node.style.direction == UIDirection::Column) {
                totalChildrenSize += child.style.height.has_value() ? *child.style.height : measure_content_height(child);
            } else {
                totalChildrenSize += child.style.width.has_value() ? *child.style.width : measure_content_width(child);
            }
        }
        if (node.children.size() > 1) {
            totalChildrenSize += static_cast<float>((node.children.size() - 1) * node.style.gap);
        }

        // Calculate starting position based on justify-content
        float curX = node.computed_rect.x + px;
        float curY = node.computed_rect.y + py;

        const float mainAxisSize = (node.style.direction == UIDirection::Column) ? ph : pw;
        float mainAxisStart = 0;

        switch (node.style.justify_content) {
            case UIAlign::Start:
                mainAxisStart = 0;
                break;
            case UIAlign::Center:
                mainAxisStart = (mainAxisSize - totalChildrenSize) / 2;
                break;
            case UIAlign::End:
                mainAxisStart = mainAxisSize - totalChildrenSize;
                break;
        }

        if (node.style.direction == UIDirection::Column) {
            curY += mainAxisStart;
        } else {
            curX += mainAxisStart;
        }

        for (auto& child : node.children) {
            // Get child size
            float childW = child.style.width.has_value() ? static_cast<float>(*child.style.width) : static_cast<float>(measure_content_width(child));
            float childH = child.style.height.has_value() ? static_cast<float>(*child.style.height) : static_cast<float>(measure_content_height(child));

            // Apply align-items (cross-axis alignment)
            float alignedX = curX;
            float alignedY = curY;

            if (node.style.direction == UIDirection::Column) {
                // Cross axis is horizontal
                switch (node.style.align_items) {
                    case UIAlign::Start:
                        alignedX = curX;
                        break;
                    case UIAlign::Center:
                        alignedX = curX + (pw - childW) / 2;
                        break;
                    case UIAlign::End:
                        alignedX = curX + pw - childW;
                        break;
                }
            } else {
                // Cross axis is vertical
                switch (node.style.align_items) {
                    case UIAlign::Start:
                        alignedY = curY;
                        break;
                    case UIAlign::Center:
                        alignedY = curY + (ph - childH) / 2;
                        break;
                    case UIAlign::End:
                        alignedY = curY + ph - childH;
                        break;
                }
            }

            rf::Rect childAvail{alignedX, alignedY, childW, childH};
            layout(child, childAvail, vm);

            if (node.style.direction == UIDirection::Column) {
                curY += child.computed_rect.h + node.style.gap;
            } else {
                curX += child.computed_rect.w + node.style.gap;
            }
        }
    }
}

bool UIDocument::update_node_rec(Node& node, rf::Vec2 mouse_pos, bool mouse_down, bool mouse_pressed) {
    bool captured = false;

    // Check if mouse is over this node
    const bool over = (mouse_pos.x >= node.computed_rect.x && 
                       mouse_pos.x <= node.computed_rect.x + node.computed_rect.w &&
                       mouse_pos.y >= node.computed_rect.y && 
                       mouse_pos.y <= node.computed_rect.y + node.computed_rect.h);
    
    // Notify script about hover state changes
    if (over != node.hovered && !node.id.empty()) {
        notify_script_hover(node.id, over);
    }
    node.hovered = over;

    if (node.type == "Button" && over) {
        captured = true;
        node.pressed = mouse_down;

        if (mouse_pressed) {
            // Notify C++ callback
            if (on_click_) {
                const std::string click_id = node.action.empty() ? node.id : node.action;
                if (!click_id.empty()) {
                    on_click_(click_id);
                }
            }
            // Notify script engine
            if (!node.id.empty()) {
                notify_script_click(node.id);
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

bool UIDocument::update(const UIViewModel& vm, rf::Vec2 mouse_pos, bool mouse_down, bool mouse_pressed) {
    if (!loaded_) {
        return false;
    }

    // Run layout pass
    rf::Rect full_screen{0, 0, static_cast<float>(vm.screen_width), static_cast<float>(vm.screen_height)};
    (void)full_screen;
    for (auto& child : root_.children) {
        int w = child.style.width.has_value() ? *child.style.width : measure_content_width(child);
        int h = child.style.height.has_value() ? *child.style.height : measure_content_height(child);
        rf::Rect positioned = anchor_rect(child.style, w, h, vm.screen_width, vm.screen_height);
        layout(child, positioned, vm);
    }

    // Update interaction state
    bool captured = false;
    for (auto& child : root_.children) {
        if (update_node_rec(child, mouse_pos, mouse_down, mouse_pressed)) {
            captured = true;
        }
    }

    // Update script engine (for animations, timers, etc.)
    if (scriptEngine_ && scriptEngine_->has_scripts()) {
        scriptEngine_->update(vm.dt > 0.0f ? vm.dt : 0.016f);
        process_script_commands();
    }

    return captured;
}

void UIDocument::render(const UIViewModel& vm) {
    if (!loaded_) {
        return;
    }

    TraceLog(LOG_DEBUG, "[UIDocument::render] children=%d", static_cast<int>(root_.children.size()));
    int idx = 0;
    for (const auto& child : root_.children) {
        TraceLog(LOG_DEBUG, "[UIDocument::render] child[%d] type=%s", idx, child.type.c_str());
        render_node(child, vm);
        TraceLog(LOG_DEBUG, "[UIDocument::render] child[%d] done", idx);
        idx++;
    }
    TraceLog(LOG_DEBUG, "[UIDocument::render] all done");
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
    const rf::Rect& r = node.computed_rect;

    TraceLog(LOG_DEBUG, "[UIDocument::render_panel] shadow");
    draw_box_shadow(r, node.style);

    auto& batch = rf::Batch2D::instance();

    // Background
    if (node.style.background_color.has_value()) {
        TraceLog(LOG_DEBUG, "[UIDocument::render_panel] bg");
        rf::Color bgColor = apply_opacity(*node.style.background_color, node.style.opacity);
        if (node.style.border_radius > 0) {
            float roundness = static_cast<float>(node.style.border_radius) / std::min(r.w, r.h);
            batch.drawRectRounded(r.x, r.y, r.w, r.h, roundness, 8, bgColor);
        } else {
            batch.drawRect(r.x, r.y, r.w, r.h, bgColor);
        }
    }

    // Border
    if (node.style.border_width > 0) {
        TraceLog(LOG_DEBUG, "[UIDocument::render_panel] border");
        rf::Color borderColor = apply_opacity(node.style.border_color, node.style.opacity);
        if (node.style.border_radius > 0) {
            float roundness = static_cast<float>(node.style.border_radius) / std::min(r.w, r.h);
            batch.drawRectRoundedLines(r.x, r.y, r.w, r.h, roundness, 8, static_cast<float>(node.style.border_width), borderColor);
        } else {
            batch.drawRectLinesEx(r.x, r.y, r.w, r.h, static_cast<float>(node.style.border_width), borderColor);
        }
    }

    // Render children
    TraceLog(LOG_DEBUG, "[UIDocument::render_panel] children=%d", static_cast<int>(node.children.size()));
    int ci = 0;
    for (const auto& child : node.children) {
        TraceLog(LOG_DEBUG, "[UIDocument::render_panel] child[%d] type=%s", ci, child.type.c_str());
        render_node(child, vm);
        ci++;
    }
}

void UIDocument::render_text(const Node& node, const UIViewModel& vm) {
    (void)vm;
    TraceLog(LOG_DEBUG, "[UIDocument::render_text] '%s'", node.text.c_str());

    if (node.text.empty()) {
        return;
    }

    const int fontSize = node.style.font_size;
    const rf::Color color = to_rf_color(node.style.color);
    auto& batch = rf::Batch2D::instance();
    const int textW = static_cast<int>(batch.measureText(node.text, static_cast<float>(fontSize)));
    const rf::Rect& r = node.computed_rect;

    // Horizontal alignment
    int tx = static_cast<int>(r.x);
    switch (node.style.text_align) {
        case UITextAlign::Left:
            tx = static_cast<int>(r.x);
            break;
        case UITextAlign::Center:
            tx = static_cast<int>(r.x + (r.w - textW) / 2);
            break;
        case UITextAlign::Right:
            tx = static_cast<int>(r.x + r.w - textW);
            break;
    }

    // Vertical alignment
    int ty = static_cast<int>(r.y);
    switch (node.style.vertical_align) {
        case UIVerticalAlign::Top:
            ty = static_cast<int>(r.y);
            break;
        case UIVerticalAlign::Middle:
            ty = static_cast<int>(r.y + (r.h - fontSize) / 2);
            break;
        case UIVerticalAlign::Bottom:
            ty = static_cast<int>(r.y + r.h - fontSize);
            break;
    }

    batch.drawText(node.text, static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(fontSize), color);
}

void UIDocument::render_button(const Node& node, const UIViewModel& vm) {
    (void)vm;
    TraceLog(LOG_DEBUG, "[UIDocument::render_button] '%s'", node.text.c_str());

    const rf::Rect& r = node.computed_rect;

    draw_box_shadow(r, node.style);

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

    auto& batch = rf::Batch2D::instance();
    rf::Color bgColor = apply_opacity(bg, node.style.opacity);
    if (node.style.border_radius > 0) {
        float roundness = static_cast<float>(node.style.border_radius) / std::min(r.w, r.h);
        batch.drawRectRounded(r.x, r.y, r.w, r.h, roundness, 8, bgColor);
    } else {
        batch.drawRect(r.x, r.y, r.w, r.h, bgColor);
    }

    // Border
    if (node.style.border_width > 0) {
        UIColor border_col = node.style.border_color;
        if (node.hovered) {
            border_col.r = static_cast<unsigned char>(std::min(255, border_col.r + 50));
            border_col.g = static_cast<unsigned char>(std::min(255, border_col.g + 50));
            border_col.b = static_cast<unsigned char>(std::min(255, border_col.b + 50));
        }
        rf::Color borderColor = apply_opacity(border_col, node.style.opacity);
        if (node.style.border_radius > 0) {
            float roundness = static_cast<float>(node.style.border_radius) / std::min(r.w, r.h);
            batch.drawRectRoundedLines(r.x, r.y, r.w, r.h, roundness, 8, static_cast<float>(node.style.border_width), borderColor);
        } else {
            batch.drawRectLinesEx(r.x, r.y, r.w, r.h, static_cast<float>(node.style.border_width), borderColor);
        }
    }

    if (!node.text.empty()) {
        const int fontSize = node.style.font_size;
        const rf::Color textColor = to_rf_color(node.style.color);
        const int textW = static_cast<int>(batch.measureText(node.text, static_cast<float>(fontSize)));
        const int textH = fontSize;
        
        const float innerX = r.x + node.style.padding.left;
        const float innerY = r.y + node.style.padding.top;
        const float innerW = r.w - node.style.padding.left - node.style.padding.right;
        const float innerH = r.h - node.style.padding.top - node.style.padding.bottom;
        
        int tx = static_cast<int>(innerX);
        switch (node.style.text_align) {
            case UITextAlign::Left:
                tx = static_cast<int>(innerX);
                break;
            case UITextAlign::Center:
                tx = static_cast<int>(innerX + (innerW - textW) / 2);
                break;
            case UITextAlign::Right:
                tx = static_cast<int>(innerX + innerW - textW);
                break;
        }
        
        int ty = static_cast<int>(innerY);
        switch (node.style.vertical_align) {
            case UIVerticalAlign::Top:
                ty = static_cast<int>(innerY);
                break;
            case UIVerticalAlign::Middle:
                ty = static_cast<int>(innerY + (innerH - textH) / 2);
                break;
            case UIVerticalAlign::Bottom:
                ty = static_cast<int>(innerY + innerH - textH);
                break;
        }
        
        batch.drawText(node.text, static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(fontSize), textColor);
    }
}

void UIDocument::render_health_bar(const Node& node, const UIViewModel& vm) {
    rf::ITexture* full = load_texture_cached(node.full);
    rf::ITexture* half = load_texture_cached(node.half);
    rf::ITexture* empty_tex = load_texture_cached(node.empty);

    if (!full || !half || !empty_tex) {
        return;
    }

    const int maxHealth = (vm.player.max_health <= 0) ? 20 : vm.player.max_health;
    int health = vm.player.health;
    if (health < 0) health = 0;
    if (health > maxHealth) health = maxHealth;

    const int hearts = (maxHealth + 1) / 2;
    const int fullHearts = health / 2;
    const bool hasHalf = (health % 2) != 0;

    const int heartW = node.style.width.has_value() ? *node.style.width : 16;
    const int heartH = node.style.height.has_value() ? *node.style.height : 16;

    const int gap = node.style.gap;
    const int contentW = hearts * heartW + (hearts > 0 ? (hearts - 1) * gap : 0);
    const int contentH = heartH;

    const rf::Rect r = anchor_rect(node.style, contentW, contentH, vm.screen_width, vm.screen_height);

    for (int i = 0; i < hearts; i++) {
        const int x = static_cast<int>(r.x) + i * (heartW + gap);
        const int y = static_cast<int>(r.y);

        auto tex = empty_tex;
        if (i < fullHearts) {
            tex = full;
        } else if (i == fullHearts && hasHalf) {
            tex = half;
        }

        const rf::Rect src{0.0f, 0.0f, static_cast<float>(tex->width()), static_cast<float>(tex->height())};
        const rf::Rect dst{static_cast<float>(x), static_cast<float>(y), static_cast<float>(heartW), static_cast<float>(heartH)};
        rf::Batch2D::instance().drawTexture(tex, src, dst);
    }
}

void UIDocument::parse_script_element(tinyxml2::XMLElement* el) {
    // Create script engine if not exists
    auto ensure_script_engine = [&]() -> bool {
        if (!scriptEngine_) {
            scriptEngine_ = std::make_unique<scripting::UIScriptEngine>();
            if (!scriptEngine_->init()) {
                TraceLog(LOG_ERROR, "[ui] Failed to initialize UI script engine");
                scriptEngine_.reset();
                return false;
            }
            if (scriptLogCallback_) {
                scriptEngine_->set_log_callback(scriptLogCallback_);
            }
        }
        return true;
    };
    
    // Check for src attribute: <Script src="scripts/client/ui/button_logic.lua"/>
    const char* srcAttr = el->Attribute("src");
    if (srcAttr && std::string(srcAttr).length() > 0) {
        // Load external script from VFS
        auto content = engine::vfs::read_text_file(srcAttr);
        if (!content) {
            TraceLog(LOG_ERROR, "[ui] Script src not found: %s", srcAttr);
            return;
        }
        
        if (!ensure_script_engine()) return;
        
        auto result = scriptEngine_->load_script(*content, srcAttr);
        if (!result) {
            TraceLog(LOG_ERROR, "[ui] Failed to load UI script '%s': %s", srcAttr, result.error.c_str());
        }
        return;
    }
    
    // Fall back to inline script content: <Script>...code...</Script>
    const char* text = el->GetText();
    if (!text || std::string(text).empty()) {
        return;
    }
    
    if (!ensure_script_engine()) return;
    
    // Load the script
    auto result = scriptEngine_->load_script(text, "inline_script");
    if (!result) {
        TraceLog(LOG_ERROR, "[ui] Failed to load UI script: %s", result.error.c_str());
    }
}

void UIDocument::process_script_commands() {
    if (!scriptEngine_) return;
    
    auto commands = scriptEngine_->take_commands();
    for (const auto& cmd : commands) {
        switch (cmd.type) {
            case scripting::UICommand::Type::Show:
                // TODO: Implement show element
                TraceLog(LOG_DEBUG, "[ui script] show: %s", cmd.elementId.c_str());
                break;
                
            case scripting::UICommand::Type::Hide:
                // TODO: Implement hide element
                TraceLog(LOG_DEBUG, "[ui script] hide: %s", cmd.elementId.c_str());
                break;
                
            case scripting::UICommand::Type::SetText:
                // TODO: Implement set text
                TraceLog(LOG_DEBUG, "[ui script] set_text: %s = %s", 
                         cmd.elementId.c_str(), cmd.stringParam.c_str());
                break;
                
            case scripting::UICommand::Type::SetStyle:
                // TODO: Implement set style
                TraceLog(LOG_DEBUG, "[ui script] set_style: %s.%s = %s", 
                         cmd.elementId.c_str(), cmd.stringParam.c_str(), cmd.stringParam2.c_str());
                break;
                
            case scripting::UICommand::Type::PlaySound:
                // TODO: Implement play sound
                TraceLog(LOG_DEBUG, "[ui script] play_sound: %s", cmd.stringParam.c_str());
                break;
                
            case scripting::UICommand::Type::Animate:
                // TODO: Implement animation
                TraceLog(LOG_DEBUG, "[ui script] animate: %s (%s, %.2f)", 
                         cmd.elementId.c_str(), cmd.stringParam.c_str(), cmd.floatParams[0]);
                break;
                
            default:
                break;
        }
    }
}

void UIDocument::notify_script_click(const std::string& elementId) {
    if (scriptEngine_ && scriptEngine_->has_scripts()) {
        scriptEngine_->on_click(elementId);
    }
}

void UIDocument::notify_script_hover(const std::string& elementId, bool hovered) {
    if (scriptEngine_ && scriptEngine_->has_scripts()) {
        scriptEngine_->on_hover(elementId, hovered);
    }
}

void UIDocument::set_script_log_callback(std::function<void(const std::string&)> callback) {
    scriptLogCallback_ = std::move(callback);
    if (scriptEngine_) {
        scriptEngine_->set_log_callback(scriptLogCallback_);
    }
}

} // namespace ui::xmlui
