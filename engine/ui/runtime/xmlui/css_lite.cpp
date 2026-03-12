#include "css_lite.hpp"

#include <cctype>
#include <optional>
#include <utility>

namespace ui::xmlui {

static void skip_ws_and_comments(std::string_view s, size_t& i) {
    while (i < s.size()) {
        // Skip whitespace
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
            i++;
        }
        // Skip C-style comments /* ... */
        if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
                i++;
            }
            if (i + 1 < s.size()) {
                i += 2; // skip */
            }
            continue;
        }
        break;
    }
}

static void skip_ws(std::string_view s, size_t& i) {
    skip_ws_and_comments(s, i);
}

static bool consume(std::string_view s, size_t& i, char c) {
    skip_ws(s, i);
    if (i < s.size() && s[i] == c) {
        i++;
        return true;
    }
    return false;
}

static std::string parse_ident(std::string_view s, size_t& i) {
    skip_ws(s, i);
    size_t start = i;
    while (i < s.size()) {
        char ch = s[i];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_') {
            i++;
            continue;
        }
        break;
    }
    return std::string{s.substr(start, i - start)};
}

static std::string parse_until(std::string_view s, size_t& i, char end) {
    skip_ws(s, i);
    size_t start = i;
    while (i < s.size() && s[i] != end) {
        i++;
    }
    return std::string{s.substr(start, i - start)};
}

static std::optional<int> parse_int(std::string_view s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    bool neg = false;
    if (i < s.size() && s[i] == '-') {
        neg = true;
        i++;
    }
    int v = 0;
    bool any = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        any = true;
        v = v * 10 + (s[i] - '0');
        i++;
    }
    if (!any) return std::nullopt;
    if (neg) v = -v;
    return v;
}

static void set_box_all(UIBox& b, int v) {
    b.left = v;
    b.top = v;
    b.right = v;
    b.bottom = v;
}

static std::optional<UIAnchor> parse_anchor(std::string_view v) {
    auto norm = [](std::string_view x) {
        std::string out;
        out.reserve(x.size());
        for (char c : x) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        return out;
    };

    const std::string s = norm(v);
    if (s == "top-left" || s == "topleft") return UIAnchor::TopLeft;
    if (s == "top") return UIAnchor::Top;
    if (s == "top-right" || s == "topright") return UIAnchor::TopRight;
    if (s == "left") return UIAnchor::Left;
    if (s == "center") return UIAnchor::Center;
    if (s == "right") return UIAnchor::Right;
    if (s == "bottom-left" || s == "bottomleft") return UIAnchor::BottomLeft;
    if (s == "bottom") return UIAnchor::Bottom;
    if (s == "bottom-right" || s == "bottomright") return UIAnchor::BottomRight;
    return std::nullopt;
}

static std::optional<UIDirection> parse_direction(std::string_view v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (s == "row") return UIDirection::Row;
    if (s == "column") return UIDirection::Column;
    return std::nullopt;
}

static std::optional<UIAlign> parse_align(std::string_view v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (s == "start" || s == "flex-start") return UIAlign::Start;
    if (s == "center") return UIAlign::Center;
    if (s == "end" || s == "flex-end") return UIAlign::End;
    return std::nullopt;
}

static std::optional<UITextAlign> parse_text_align(std::string_view v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (s == "left" || s == "start") return UITextAlign::Left;
    if (s == "center") return UITextAlign::Center;
    if (s == "right" || s == "end") return UITextAlign::Right;
    return std::nullopt;
}

static std::optional<UIVerticalAlign> parse_vertical_align(std::string_view v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (s == "top" || s == "start") return UIVerticalAlign::Top;
    if (s == "middle" || s == "center") return UIVerticalAlign::Middle;
    if (s == "bottom" || s == "end") return UIVerticalAlign::Bottom;
    return std::nullopt;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static std::optional<UIColor> parse_color(std::string_view v) {
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.remove_prefix(1);
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back()))) v.remove_suffix(1);

    std::string lower;
    lower.reserve(v.size());
    for (char c : v) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lower == "transparent") return UIColor{0, 0, 0, 0};
    if (lower == "black") return UIColor{0, 0, 0, 255};
    if (lower == "white") return UIColor{255, 255, 255, 255};
    if (lower == "red") return UIColor{255, 0, 0, 255};
    if (lower == "green") return UIColor{0, 255, 0, 255};
    if (lower == "blue") return UIColor{0, 0, 255, 255};
    if (lower == "yellow") return UIColor{255, 255, 0, 255};
    if (lower == "gray" || lower == "grey") return UIColor{128, 128, 128, 255};
    if (lower == "darkgray" || lower == "darkgrey") return UIColor{64, 64, 64, 255};
    if (lower == "lightgray" || lower == "lightgrey") return UIColor{192, 192, 192, 255};

    // #RGB, #RGBA, #RRGGBB, #RRGGBBAA
    if (!v.empty() && v[0] == '#') {
        v.remove_prefix(1);
        if (v.size() == 3) {
            int r = hex_digit(v[0]);
            int g = hex_digit(v[1]);
            int b = hex_digit(v[2]);
            if (r >= 0 && g >= 0 && b >= 0) {
                return UIColor{
                    static_cast<unsigned char>(r * 17),
                    static_cast<unsigned char>(g * 17),
                    static_cast<unsigned char>(b * 17),
                    255
                };
            }
        } else if (v.size() == 4) {
            int r = hex_digit(v[0]);
            int g = hex_digit(v[1]);
            int b = hex_digit(v[2]);
            int a = hex_digit(v[3]);
            if (r >= 0 && g >= 0 && b >= 0 && a >= 0) {
                return UIColor{
                    static_cast<unsigned char>(r * 17),
                    static_cast<unsigned char>(g * 17),
                    static_cast<unsigned char>(b * 17),
                    static_cast<unsigned char>(a * 17)
                };
            }
        } else if (v.size() == 6) {
            int r = hex_digit(v[0]) * 16 + hex_digit(v[1]);
            int g = hex_digit(v[2]) * 16 + hex_digit(v[3]);
            int b = hex_digit(v[4]) * 16 + hex_digit(v[5]);
            if (r >= 0 && g >= 0 && b >= 0) {
                return UIColor{
                    static_cast<unsigned char>(r),
                    static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b),
                    255
                };
            }
        } else if (v.size() == 8) {
            int r = hex_digit(v[0]) * 16 + hex_digit(v[1]);
            int g = hex_digit(v[2]) * 16 + hex_digit(v[3]);
            int b = hex_digit(v[4]) * 16 + hex_digit(v[5]);
            int a = hex_digit(v[6]) * 16 + hex_digit(v[7]);
            if (r >= 0 && g >= 0 && b >= 0 && a >= 0) {
                return UIColor{
                    static_cast<unsigned char>(r),
                    static_cast<unsigned char>(g),
                    static_cast<unsigned char>(b),
                    static_cast<unsigned char>(a)
                };
            }
        }
    }

    return std::nullopt;
}

static bool parse_bool(std::string_view v) {
    std::string s;
    s.reserve(v.size());
    for (char c : v) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return s == "true" || s == "1" || s == "yes";
}

// Helper to check if a class list (space-separated) contains a specific class
static bool class_list_contains(std::string_view class_list, std::string_view target_class) {
    if (class_list.empty() || target_class.empty()) return false;
    
    size_t pos = 0;
    while (pos < class_list.size()) {
        // Skip leading whitespace
        while (pos < class_list.size() && std::isspace(static_cast<unsigned char>(class_list[pos]))) {
            ++pos;
        }
        if (pos >= class_list.size()) break;
        
        // Find end of this class name
        size_t start = pos;
        while (pos < class_list.size() && !std::isspace(static_cast<unsigned char>(class_list[pos]))) {
            ++pos;
        }
        
        // Check if this class matches
        std::string_view current_class = class_list.substr(start, pos - start);
        if (current_class == target_class) {
            return true;
        }
    }
    return false;
}

static bool selector_matches(const CSSSelector& sel, std::string_view type, std::string_view id, std::string_view class_name) {
    switch (sel.kind) {
        case CSSSelector::Kind::Type: return sel.value == type;
        case CSSSelector::Kind::Id: return sel.value == id;
        case CSSSelector::Kind::Class: return class_list_contains(class_name, sel.value);
    }
    return false;
}

CSSParseResult parse_css_lite(std::string_view css) {
    CSSParseResult out;

    size_t i = 0;
    while (true) {
        skip_ws(css, i);
        if (i >= css.size()) break;

        // selector
        CSSRule rule;
        if (css[i] == '#') {
            i++;
            rule.selector.kind = CSSSelector::Kind::Id;
            rule.selector.value = parse_ident(css, i);
        } else if (css[i] == '.') {
            i++;
            rule.selector.kind = CSSSelector::Kind::Class;
            rule.selector.value = parse_ident(css, i);
        } else {
            rule.selector.kind = CSSSelector::Kind::Type;
            rule.selector.value = parse_ident(css, i);
        }

        if (rule.selector.value.empty()) {
            out.error = "CSS parse error: expected selector";
            return out;
        }

        if (!consume(css, i, '{')) {
            out.error = "CSS parse error: expected '{'";
            return out;
        }

        while (true) {
            skip_ws(css, i);
            if (i >= css.size()) {
                out.error = "CSS parse error: unexpected end of file";
                return out;
            }
            if (css[i] == '}') {
                i++;
                break;
            }

            const std::string key = parse_ident(css, i);
            if (key.empty()) {
                out.error = "CSS parse error: expected property name";
                return out;
            }
            if (!consume(css, i, ':')) {
                out.error = "CSS parse error: expected ':'";
                return out;
            }

            const std::string value = parse_until(css, i, ';');
            if (!consume(css, i, ';')) {
                out.error = "CSS parse error: expected ';'";
                return out;
            }

            if (key == "width") {
                if (auto v = parse_int(value); v.has_value()) rule.style.width = *v;
            } else if (key == "height") {
                if (auto v = parse_int(value); v.has_value()) rule.style.height = *v;
            } else if (key == "gap") {
                if (auto v = parse_int(value); v.has_value()) { rule.style.gap = *v; rule.set_props |= CSSProp::Gap; }
            } else if (key == "margin") {
                if (auto v = parse_int(value); v.has_value()) { set_box_all(rule.style.margin, *v); rule.set_props |= CSSProp::Margin; }
            } else if (key == "padding") {
                if (auto v = parse_int(value); v.has_value()) { set_box_all(rule.style.padding, *v); rule.set_props |= CSSProp::Padding; }
            } else if (key == "anchor") {
                if (auto a = parse_anchor(value); a.has_value()) { rule.style.anchor = *a; rule.set_props |= CSSProp::Anchor; }
            } else if (key == "direction" || key == "flex-direction") {
                if (auto d = parse_direction(value); d.has_value()) { rule.style.direction = *d; rule.set_props |= CSSProp::Direction; }
            } else if (key == "align-items") {
                if (auto a = parse_align(value); a.has_value()) { rule.style.align_items = *a; rule.set_props |= CSSProp::AlignItems; }
            } else if (key == "justify-content") {
                if (auto a = parse_align(value); a.has_value()) { rule.style.justify_content = *a; rule.set_props |= CSSProp::Justify; }
            } else if (key == "grow" || key == "flex-grow") {
                rule.style.grow = parse_bool(value); rule.set_props |= CSSProp::Grow;
            } else if (key == "font-size") {
                if (auto v = parse_int(value); v.has_value()) { rule.style.font_size = *v; rule.set_props |= CSSProp::FontSize; }
            } else if (key == "color") {
                if (auto c = parse_color(value); c.has_value()) { rule.style.color = *c; rule.set_props |= CSSProp::Color; }
            } else if (key == "text-align") {
                if (auto a = parse_text_align(value); a.has_value()) { rule.style.text_align = *a; rule.set_props |= CSSProp::TextAlign; }
            } else if (key == "vertical-align") {
                if (auto a = parse_vertical_align(value); a.has_value()) { rule.style.vertical_align = *a; rule.set_props |= CSSProp::VAlign; }
            } else if (key == "background-color" || key == "background") {
                if (auto c = parse_color(value); c.has_value()) rule.style.background_color = c;
            } else if (key == "border-width") {
                if (auto v = parse_int(value); v.has_value()) { rule.style.border_width = *v; rule.set_props |= CSSProp::BorderWidth; }
            } else if (key == "border-color") {
                if (auto c = parse_color(value); c.has_value()) { rule.style.border_color = *c; rule.set_props |= CSSProp::BorderColor; }
            } else if (key == "border-radius") {
                if (auto v = parse_int(value); v.has_value()) { rule.style.border_radius = *v; rule.set_props |= CSSProp::BorderRadius; }
            } else if (key == "visible") {
                rule.style.visible = parse_bool(value); rule.set_props |= CSSProp::Visible;
            } else if (key == "opacity") {
                try {
                    float opacity = std::stof(std::string(value));
                    rule.style.opacity = std::max(0.0f, std::min(1.0f, opacity));
                    rule.set_props |= CSSProp::Opacity;
                } catch (...) {}
            } else if (key == "box-shadow") {
                rule.style.has_shadow = true;
                rule.set_props |= CSSProp::Shadow;
                size_t pos = 0;
                std::string val_str{value};
                // Skip whitespace
                while (pos < val_str.size() && std::isspace(static_cast<unsigned char>(val_str[pos]))) pos++;
                
                // Parse offsetX
                size_t start = pos;
                while (pos < val_str.size() && (std::isdigit(static_cast<unsigned char>(val_str[pos])) || val_str[pos] == '-')) pos++;
                if (pos > start) {
                    rule.style.shadow_offset_x = std::stoi(val_str.substr(start, pos - start));
                }
                
                // Skip whitespace
                while (pos < val_str.size() && std::isspace(static_cast<unsigned char>(val_str[pos]))) pos++;
                
                // Parse offsetY
                start = pos;
                while (pos < val_str.size() && (std::isdigit(static_cast<unsigned char>(val_str[pos])) || val_str[pos] == '-')) pos++;
                if (pos > start) {
                    rule.style.shadow_offset_y = std::stoi(val_str.substr(start, pos - start));
                }
                
                // Skip whitespace
                while (pos < val_str.size() && std::isspace(static_cast<unsigned char>(val_str[pos]))) pos++;
                
                // Parse blur
                start = pos;
                while (pos < val_str.size() && std::isdigit(static_cast<unsigned char>(val_str[pos]))) pos++;
                if (pos > start) {
                    rule.style.shadow_blur = std::stoi(val_str.substr(start, pos - start));
                }
                
                // Skip whitespace
                while (pos < val_str.size() && std::isspace(static_cast<unsigned char>(val_str[pos]))) pos++;
                
                // Parse color (rest of string)
                if (pos < val_str.size()) {
                    if (auto c = parse_color(val_str.substr(pos)); c.has_value()) {
                        rule.style.shadow_color = *c;
                    }
                }
            }
        }

        out.rules.push_back(std::move(rule));
    }

    return out;
}

UIStyle compute_style(
    const std::vector<CSSRule>& rules,
    std::string_view type,
    std::string_view id,
    std::string_view class_name) {
    UIStyle out;

    // Apply in order; later rules override earlier.
    for (const auto& r : rules) {
        if (!selector_matches(r.selector, type, id, class_name)) continue;

        const uint32_t s = r.set_props;

        if (r.style.width.has_value())  out.width = r.style.width;
        if (r.style.height.has_value()) out.height = r.style.height;

        if (s & CSSProp::Margin)       out.margin = r.style.margin;
        if (s & CSSProp::Padding)      out.padding = r.style.padding;
        if (s & CSSProp::Gap)          out.gap = r.style.gap;
        if (s & CSSProp::Anchor)       out.anchor = r.style.anchor;

        if (s & CSSProp::Direction)    out.direction = r.style.direction;
        if (s & CSSProp::AlignItems)   out.align_items = r.style.align_items;
        if (s & CSSProp::Justify)      out.justify_content = r.style.justify_content;
        if (s & CSSProp::Grow)         out.grow = r.style.grow;

        if (s & CSSProp::FontSize)     out.font_size = r.style.font_size;
        if (s & CSSProp::Color)        out.color = r.style.color;
        if (s & CSSProp::TextAlign)    out.text_align = r.style.text_align;
        if (s & CSSProp::VAlign)       out.vertical_align = r.style.vertical_align;
        if (r.style.background_color.has_value()) out.background_color = r.style.background_color;

        if (s & CSSProp::BorderWidth)  out.border_width = r.style.border_width;
        if (s & CSSProp::BorderColor)  out.border_color = r.style.border_color;
        if (s & CSSProp::BorderRadius) out.border_radius = r.style.border_radius;
        if (s & CSSProp::Visible)      out.visible = r.style.visible;
        if (s & CSSProp::Opacity)      out.opacity = r.style.opacity;

        if (s & CSSProp::Shadow) {
            out.has_shadow = r.style.has_shadow;
            out.shadow_offset_x = r.style.shadow_offset_x;
            out.shadow_offset_y = r.style.shadow_offset_y;
            out.shadow_blur = r.style.shadow_blur;
            out.shadow_color = r.style.shadow_color;
        }
    }

    return out;
}

} // namespace ui::xmlui
