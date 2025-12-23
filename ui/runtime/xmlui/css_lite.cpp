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

static bool selector_matches(const CSSSelector& sel, std::string_view type, std::string_view id, std::string_view class_name) {
    switch (sel.kind) {
        case CSSSelector::Kind::Type: return sel.value == type;
        case CSSSelector::Kind::Id: return sel.value == id;
        case CSSSelector::Kind::Class: return sel.value == class_name;
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
                if (auto v = parse_int(value); v.has_value()) rule.style.gap = *v;
            } else if (key == "margin") {
                if (auto v = parse_int(value); v.has_value()) set_box_all(rule.style.margin, *v);
            } else if (key == "padding") {
                if (auto v = parse_int(value); v.has_value()) set_box_all(rule.style.padding, *v);
            } else if (key == "anchor") {
                if (auto a = parse_anchor(value); a.has_value()) rule.style.anchor = *a;
            } else if (key == "direction" || key == "flex-direction") {
                if (auto d = parse_direction(value); d.has_value()) rule.style.direction = *d;
            } else if (key == "align-items") {
                if (auto a = parse_align(value); a.has_value()) rule.style.align_items = *a;
            } else if (key == "justify-content") {
                if (auto a = parse_align(value); a.has_value()) rule.style.justify_content = *a;
            } else if (key == "grow" || key == "flex-grow") {
                rule.style.grow = parse_bool(value);
            } else if (key == "font-size") {
                if (auto v = parse_int(value); v.has_value()) rule.style.font_size = *v;
            } else if (key == "color") {
                if (auto c = parse_color(value); c.has_value()) rule.style.color = *c;
            } else if (key == "background-color" || key == "background") {
                if (auto c = parse_color(value); c.has_value()) rule.style.background_color = c;
            } else if (key == "border-width") {
                if (auto v = parse_int(value); v.has_value()) rule.style.border_width = *v;
            } else if (key == "border-color") {
                if (auto c = parse_color(value); c.has_value()) rule.style.border_color = *c;
            } else if (key == "border-radius") {
                if (auto v = parse_int(value); v.has_value()) rule.style.border_radius = *v;
            } else if (key == "visible") {
                rule.style.visible = parse_bool(value);
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

        if (r.style.width.has_value()) out.width = r.style.width;
        if (r.style.height.has_value()) out.height = r.style.height;

        out.margin = r.style.margin;
        out.padding = r.style.padding;
        out.gap = r.style.gap;
        out.anchor = r.style.anchor;

        out.direction = r.style.direction;
        out.align_items = r.style.align_items;
        out.justify_content = r.style.justify_content;
        out.grow = r.style.grow;

        out.font_size = r.style.font_size;
        out.color = r.style.color;
        if (r.style.background_color.has_value()) out.background_color = r.style.background_color;

        out.border_width = r.style.border_width;
        out.border_color = r.style.border_color;
        out.border_radius = r.style.border_radius;
        out.visible = r.style.visible;
    }

    return out;
}

} // namespace ui::xmlui
